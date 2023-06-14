#include "./header.h"
#include "../gpt4all-backend/llmodel_c.h"
#include "./utils.h"
#include "./parse_json.h"

//////////////////////////////////////////////////////////////////////////
////////////                    ANIMATION                     ////////////
//////////////////////////////////////////////////////////////////////////

std::atomic<bool> stop_display{false}; 

void display_frames() {
    const char* frames[] = {".", ":", "'", ":"};
    int frame_index = 0;
    ConsoleState con_st;
    con_st.use_color = true;
    while (!stop_display) {
        set_console_color(con_st, PROMPT);
        std::cerr << "\r" << frames[frame_index % 4] << std::flush;
        frame_index++;
        set_console_color(con_st, DEFAULT);
        if (!stop_display){
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            std::cerr << "\r" << " " << std::flush;
            std::cerr << "\r" << std::flush;
        }
    }
}

void display_loading() {

    while (!stop_display) {


        for (int i=0; i < 14; i++){
                fprintf(stdout, ".");
                fflush(stdout);
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                if (stop_display){ break; }
        }
        
        std::cout << "\r" << "               " << "\r" << std::flush;
    }
    std::cout << "\r" << " " << std::flush;

}

//////////////////////////////////////////////////////////////////////////
////////////                   /ANIMATION                     ////////////
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
////////////                 CHAT FUNCTIONS                   ////////////
//////////////////////////////////////////////////////////////////////////


bool save_state_to_binary(llmodel_model& model, uint8_t *dest, chatParams& params) {
  std::filesystem::path directory_path(params.path+"./saves");
    if (!std::filesystem::is_directory(directory_path)) {
        if (!std::filesystem::create_directory(directory_path)) {
            std::cerr << "Error creating directory" << std::endl;
            return false;
        }
    }
  // create an output file stream
  std::ofstream outfile;
  // open the file in binary mode
  outfile.open(params.path+"./saves/"+params.state+".bin", std::ios::binary);

  // check if the file stream is open
  if (!outfile.is_open()) {
    std::cerr << "Error opening file " << params.state+".bin" << std::endl;
    return false;
  }

  // write the model data to the file stream
  uint64_t copied_bytes = llmodel_save_state_data(model, dest);
  outfile.write(reinterpret_cast<char *>(dest), copied_bytes);

  // close the file stream
  outfile.close();
  return true;
}

bool load_state_from_binary(llmodel_model& model, chatParams& params) {
  // create an input file stream
  std::ifstream infile;
  // open the file in binary mode
  infile.open(params.path+"./saves/"+params.state+".bin", std::ios::binary);

  // check if the file stream is open
  if (!infile.is_open()) {
    std::cerr << "Error opening file " << params.state+".bin" << std::endl;
    return false;
  }

  // get the size of the file
  infile.seekg(0, std::ios::end);
  uint64_t file_size = infile.tellg();
  infile.seekg(0, std::ios::beg);

  // allocate a buffer to hold the file data
  uint8_t* buffer = new uint8_t[file_size];

  // read the file data into the buffer
  infile.read(reinterpret_cast<char*>(buffer), file_size);
  infile.close();

  // restore the internal state of the model using the buffer data
  llmodel_restore_state_data(model, buffer);  
  //prompt_context  = *static_cast<const llmodel_prompt_context*>(reinterpret_cast<void*>(buffer)); 
  delete[] buffer;
  return true;
}

bool save_ctx_to_binary(llmodel_prompt_context& prompt_context, chatParams& params) {
  std::filesystem::path directory_path(params.path+"./saves");
    if (!std::filesystem::is_directory(directory_path)) {
        if (!std::filesystem::create_directory(directory_path)) {
            std::cerr << "Error creating directory" << std::endl;
            return false;
        }
    }
	std::string context_filename = params.path+"./saves/"+params.state+".ctx";
    // Open the binary file for writing
    FILE* file = fopen(context_filename.c_str(), "wb");
    if (!file) {
        std::cerr << "Error opening file: " << params.state+".ctx" << std::endl;
        return false;
    }

    // Write the struct to the file using fwrite
    size_t size = sizeof(llmodel_prompt_context);
    fwrite(&prompt_context, sizeof(llmodel_prompt_context), 1, file);

    // Close the file
    fclose(file);
    return true;
}

llmodel_prompt_context load_ctx_from_binary(chatParams& params) {
	std::string context_filename = params.path+"./saves/"+params.state+".ctx";
    // Open the binary file for reading
    FILE* file = fopen(context_filename.c_str(), "rb");
    if (!file) {
        std::cerr << "Error opening file: " << params.state+".ctx" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Read the struct from the file using fread
    llmodel_prompt_context prompt_context;
    size_t size = sizeof(llmodel_prompt_context);
    fread(&prompt_context, sizeof(llmodel_prompt_context), 1, file);

    // Close the file
    fclose(file);

    return prompt_context;
}

std::string get_input(ConsoleState& con_st, std::string& input, chatParams &params, llmodel_prompt_context &prompt_context, llmodel_model& model) {
    set_console_color(con_st, USER_INPUT);

    std::cout << "\n> ";
    std::getline(std::cin, input);
    
    std::istringstream iss(input);
    std::string input1, input2;
    std::getline(iss, input1, ' ');
    std::getline(iss, input2, ' ');
    set_console_color(con_st, DEFAULT);
    
    if (input == "/reset") {
    	//reset the logits, tokens and past conversation
        prompt_context.logits = params.logits;
        prompt_context.logits_size = params.logits_size;
        prompt_context.tokens = params.tokens;
        prompt_context.tokens_size = params.tokens_size;
        prompt_context.n_past = params.n_past;
        prompt_context.n_ctx = params.n_ctx;
        
        //get new input using recursion
        set_console_color(con_st, PROMPT);
        std::cout << "Chat context reset.";
        return get_input(con_st, input, params, prompt_context, model);
    }
    if ((input == "/save" || input1 == "/save") && (params.no_saves == false)) {
    	if (input2 != "" && (input2.find("..") == std::string::npos) ) { params.state = input2; }
    	
    	
        bool success1 = false;
        bool success2 = false;
    	
    	uint64_t model_size = llmodel_get_state_size(model);
		uint8_t *dest = new uint8_t[model_size];
    	success1 = save_state_to_binary(model, dest, params);
    	delete[] dest;
    	success2 = save_ctx_to_binary(prompt_context, params);
    	
    	//get new input using recursion
        set_console_color(con_st, PROMPT);
        if (success1 && success2) { std::cout << "Model data saved to: " << "saves/"+params.state+".bin" << " size: " << floor(model_size/10000000)/100.0 << " Gb"; }
        return get_input(con_st, input, params, prompt_context, model);
    }
    
    if ((input == "/load" || input1 == "/load") && (params.no_saves == false)) {
    	if (input2 != "" && (input2.find("..") == std::string::npos) ) { params.state = input2; }
    	//reset the logits, tokens and past conversation
        prompt_context.logits = params.logits;
        prompt_context.logits_size = params.logits_size;
        prompt_context.tokens = params.tokens;
        prompt_context.tokens_size = params.tokens_size;
        prompt_context.n_past = params.n_past;
        prompt_context.n_ctx = params.n_ctx;
        
        bool success = false;
        
    	prompt_context = load_ctx_from_binary(params);
    	success = load_state_from_binary(model, params);
    	uint64_t model_size = llmodel_get_state_size(model);
    	
    	//get new input using recursion
        set_console_color(con_st, PROMPT);
        if (success) { std::cout << "Model data loaded from: " << "saves/"+params.state+".bin" << " size: " << floor(model_size/10000000)/100.0 << " Gb"; }
        return get_input(con_st, input, params, prompt_context, model);
    }
    
    if (input == "/help"){
    	set_console_color(con_st, DEFAULT);
    	std::cout << std::endl;
    	char **emptyargv = (char**)calloc(1, sizeof(char*));
    	print_usage(0, emptyargv, params);
        return get_input(con_st, input, params, prompt_context, model);
    }
        
    if (input == "exit" || input == "quit" || input == "/exit" || input == "/quit") {       
        llmodel_model_destroy(model);
        exit(0);
    }
    
    return input;
}

std::string hashstring = "";
std::string answer = "";

//////////////////////////////////////////////////////////////////////////
////////////                /CHAT FUNCTIONS                   ////////////
//////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////
////////////                  MAIN PROGRAM                    ////////////
//////////////////////////////////////////////////////////////////////////


int main(int argc, char* argv[]) {


    ConsoleState con_st;
    con_st.use_color = true;
    set_console_color(con_st, DEFAULT);

    set_console_color(con_st, PROMPT);
    set_console_color(con_st, BOLD);
    std::cout << APPNAME;
    set_console_color(con_st, DEFAULT);
    set_console_color(con_st, PROMPT);
    std::cout << " (v. " << VERSION << ")";
    set_console_color(con_st, DEFAULT);
    std::cout << "" << std::endl;
    check_avx_support_at_startup();


    chatParams params;
    //convert the default model path into Windows format if on WIN32
    #ifdef _WIN32
        std::filesystem::path p(params.model);
        params.model = p.make_preferred().string();
    #endif
 
    //get all parameters from cli arguments or json
    parse_params(argc, argv, params);
    
    //Create a prompt_context and copy all params from chatParams to prompt_context
    llmodel_prompt_context prompt_context = {
     .logits = params.logits,
     .logits_size = params.logits_size,
     .tokens = params.tokens,
     .tokens_size = params.tokens_size,
     .n_past = params.n_past,
     .n_ctx = params.n_ctx,
     .n_predict = params.n_predict,
     .top_k = params.top_k,
     .top_p = params.top_p,
     .temp = params.temp,
     .n_batch = params.n_batch,
     .repeat_penalty = params.repeat_penalty,  
     .repeat_last_n = params.repeat_last_n,
     .context_erase = params.context_erase,
    }; 
 
    //////////////////////////////////////////////////////////////////////////
    ////////////                 LOAD THE MODEL                   ////////////
    ////////////////////////////////////////////////////////////////////////// 

    //animation
    std::future<void> future;
    stop_display = true;
    if(params.use_animation) {stop_display = false; future = std::async(std::launch::async, display_loading);}

    //handle stderr for now
    //this is just to prevent printing unnecessary details during model loading.
    int stderr_copy = dup(fileno(stderr));
    #ifdef _WIN32
        std::freopen("NUL", "w", stderr);
    #else
        std::freopen("/dev/null", "w", stderr);
    #endif


    llmodel_model model = llmodel_model_create(params.model.c_str());
    std::cout << "\r" << APPNAME << ": loading " << params.model.c_str()  << std::endl;
    
    //bring back stderr for now
    dup2(stderr_copy, fileno(stderr));
    close(stderr_copy);
    
    

    //check if model is loaded
    auto check_model = llmodel_loadModel(model, params.model.c_str());

    if (check_model == false) {
        if(params.use_animation) {
            stop_display = true;
            future.wait();
            stop_display= false;
        }

        std::cerr << "Error loading: " << params.model.c_str() << std::endl;
        std::cout << "Press any key to exit..." << std::endl;
        std::cin.get();
        return 0;
    } else {
        if(params.use_animation) {
            stop_display = true;
            future.wait();
        }
        std::cout << "\r" << APPNAME << ": done loading!" << std::flush;   
    }
    //////////////////////////////////////////////////////////////////////////
    ////////////                /LOAD THE MODEL                   ////////////
    ////////////////////////////////////////////////////////////////////////// 



    set_console_color(con_st, PROMPT);
    std::cout << "\n" << params.prompt.c_str() << std::endl;
    set_console_color(con_st, DEFAULT);

    //load prompt template from file instead
    if (params.load_template != "") {
        std::tie(params.default_prefix, params.default_header, params.default_footer) = read_prompt_template_file(params.load_template);
    }
    
    //load chat log from a file
    if (params.load_log != "") {
    	if (params.prompt == "") {
        	params.prompt = params.default_prefix + read_chat_log(params.load_log) + params.default_header;
        } else {
        	params.prompt = params.default_prefix + read_chat_log(params.load_log) + params.default_header + params.prompt;
        }
    } else {
    	params.prompt = params.default_prefix + params.default_header + params.prompt;
    }
    
    //////////////////////////////////////////////////////////////////////////
    ////////////            PROMPT LAMBDA FUNCTIONS               ////////////
    //////////////////////////////////////////////////////////////////////////


    auto prompt_callback = [](int32_t token_id)  {
	    // You can handle prompt here if needed
	    return true;
	};


    auto response_callback = [](int32_t token_id, const char *responsechars) {
    
        if (!(responsechars == nullptr || responsechars[0] == '\0')) {
	    // stop the animation, printing response
        if (stop_display == false) {
	        stop_display = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            std::cerr << "\r" << " " << std::flush;
            std::cerr << "\r" << std::flush;
            if (answer != "") {std::cout << answer;}
        }
            
			std::cout << responsechars << std::flush;
	        answer += responsechars;
	    }
	            
	    return true;
	};
	
    auto recalculate_callback = [](bool is_recalculating) {
        // You can handle recalculation requests here if needed
        return is_recalculating;
    };


    //////////////////////////////////////////////////////////////////////////
    ////////////         PROMPT TEXT AND GET RESPONSE             ////////////
    //////////////////////////////////////////////////////////////////////////

    llmodel_setThreadCount(model, params.n_threads);

    std::string input = "";

    //main chat loop.
    if (!params.no_interactive) {
        input = get_input(con_st, input, params, prompt_context, model);

        //Interactive mode. We have a prompt.
        if (params.prompt != "") {
            if (params.use_animation){ stop_display = false; future = std::async(std::launch::async, display_frames); }
            if (params.b_token != ""){answer = answer + params.b_token; if(!params.use_animation) {std::cout << params.b_token;} }
            llmodel_prompt(model, (params.prompt + " " + input + params.default_footer).c_str(),
            prompt_callback, response_callback, recalculate_callback, &prompt_context);
            if (params.e_token != ""){std::cout << params.e_token; answer = answer + params.e_token; }
            if (params.use_animation){ stop_display = true; future.wait(); stop_display = false; }
            if (params.save_log != ""){ save_chat_log(params.save_log, (params.prompt + " " + input + params.default_footer).c_str(), answer.c_str()); }

        //Interactive mode. Else get prompt from input.
        } else {
            if (params.use_animation){ stop_display = false; future = std::async(std::launch::async, display_frames); }
            if (params.b_token != ""){answer = answer + params.b_token; if(!params.use_animation) {std::cout << params.b_token;} }
            llmodel_prompt(model, (params.default_prefix + params.default_header + input + params.default_footer).c_str(),
            prompt_callback, response_callback, recalculate_callback, &prompt_context);
            if (params.e_token != ""){std::cout << params.e_token; answer = answer + params.e_token; }
            if (params.use_animation){ stop_display = true; future.wait(); stop_display = false; }
            if (params.save_log != ""){ save_chat_log(params.save_log, (params.default_prefix + params.default_header + input + params.default_footer).c_str(), answer.c_str()); }
        }
        //Interactive and continuous mode. Get prompt from input.

        while (!params.run_once) {
            answer = ""; //New prompt. We stored previous answer in memory so clear it.
            input = get_input(con_st, input, params, prompt_context, model);
            if (params.use_animation){ stop_display = false; future = std::async(std::launch::async, display_frames); }
            if (params.b_token != ""){answer = answer + params.b_token; if(!params.use_animation) {std::cout << params.b_token;} }
            llmodel_prompt(model, (params.default_prefix + params.default_header + input + params.default_footer).c_str(), 
            prompt_callback, response_callback, recalculate_callback, &prompt_context);
            if (params.e_token != ""){std::cout << params.e_token; answer = answer + params.e_token; }
            if (params.use_animation){ stop_display = true; future.wait(); stop_display = false; }
            if (params.save_log != ""){ save_chat_log(params.save_log, (params.default_prefix + params.default_header + input + params.default_footer).c_str(), answer.c_str()); }

        }

    //No-interactive mode. Get the answer once from prompt and print it.
    } else {
        if (params.use_animation){ stop_display = false; future = std::async(std::launch::async, display_frames); }
        if (params.b_token != ""){answer = answer + params.b_token; if(!params.use_animation) {std::cout << params.b_token;} }
        llmodel_prompt(model, (params.prompt + params.default_footer).c_str(), 
        prompt_callback, response_callback, recalculate_callback, &prompt_context);
        if (params.e_token != ""){std::cout << params.e_token; answer = answer + params.e_token; }
        if (params.use_animation){ stop_display = true; future.wait(); stop_display = false; }
        if (params.save_log != ""){ save_chat_log(params.save_log, (params.prompt + params.default_footer).c_str(), answer.c_str()); }
        std::cout << std::endl;
    }


    set_console_color(con_st, DEFAULT);
    llmodel_model_destroy(model);
    return 0;
}
