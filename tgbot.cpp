// STT_Tg_Bot v2 Khivus 2022
//
// For credentials:
// export GOOGLE_APPLICATION_CREDENTIALS=credentials-key.json
//
// For compilation and start:
// g++ tgbot.cpp -o telegram_bot --std=c++14 -I/usr/local/include -lTgBot -lboost_system -lssl -lcrypto -lpthread -lcurl -lsqlite3 && clear && ./telegram_bot
// 
// including private token
#include "token.h" 
// Including libraries
#include <tgbot/tgbot.h>
#include <string>
#include <iostream>
#include <curl/curl.h>
#include <fstream>
#include <array>
#include <vector>
#include <sqlite3.h>
#include <nlohmann/json.hpp>
#include <ctime>
// Namespaces
using namespace std;
using namespace TgBot;
using json = nlohmann::json;
// Global variables
string chatlang;
bool callforward = false;
//
// Class trusted
//
class trusted {
private:
    fstream file;

    void write_to_file(json data) {
        file.open("config.json", ios::trunc | ios::out);
        file << data;
        file.close();
    }

public:

    bool trusted_list() {
        file.open("config.json");
        json data = json::parse(file);
        bool state = data["trusted_list"].get<bool>();
        file.close();
        return state;
    }

    void trusted_list_update(bool state) {
        file.open("config.json");
        json data = json::parse(file);
        data["trusted_list"] = state;
        file.close();
        write_to_file(data);
    }

    bool is_trusted(string username) {
        file.open("trusted-users.txt", ifstream::in);
        string user;
        while (!file.eof()) {
            file >> user;
            if (username == user) {
                file.close();
                return true;
            }
        }
        file.close();
        return false;
    }

};
//
// -------------------- Functions --------------------
//
static int callback_msg(void *data, int argc, char **argv, char **azColName) { // Function for database without output
   chatlang = argv[1];
   if (argv[0] || argv[1])
        callforward = true;
   return 0;
}

static int callback(void *data, int argc, char **argv, char **azColName) {// Function for database with output
   for(int i = 0; i < argc; i++) {
      printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
   }
   return 0;
}

bool is_chat_in_db(sqlite3* DB, Message::Ptr message) { // Funtion checks is there in db that chat
    int exit = 0;
    exit = sqlite3_exec(DB, ("SELECT * FROM chats WHERE chat_id = " + to_string(message->chat->id)).c_str(), callback_msg, 0, nullptr);
    if (exit != SQLITE_OK) 
        cerr << "Error checking db!\n";
    
    if (callforward) {
        callforward = false;
        return true;
    }
    else 
        return false;
}

string get_msg(string mode, sqlite3* DB, Message::Ptr message) { // Function for get message from file
    ifstream file;
    json data;
    string msg;

    sqlite3_exec(DB, ("SELECT * FROM chats WHERE chat_id = " + to_string(message->chat->id)).c_str(), callback_msg, 0, nullptr);

    if (chatlang == "rus")
        file.open("languages/rus.json", ifstream::in);
    else
        file.open("languages/eng.json", ifstream::in);
    
    data = json::parse(file);
    msg = data[mode].get<string>();

    file.close();

    return msg;
}

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream) { // Func for download file with curl
  size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
  return written;
}

void get_voice(string url) { // Func for get voice file
    CURL *curl;
    FILE *audiofile;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    audiofile = fopen("audio.oga", "wb"); // (re)writing to "audio.oga" file
    if(audiofile) {
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, audiofile);
        curl_easy_perform(curl);
        fclose(audiofile);
    }

    curl_easy_cleanup(curl);
    curl_global_cleanup();
}
//
// -------------------- Main() --------------------
//
int main() {
    Bot bot(token);
    // Declaring variables
    trusted Tr;
    long int admin_chat_id = 897276284;
    int msgid;
    string admin = "khivus";
    string deflang = "rus";
    bool adding_trusted = false;
    bool deleting_trusted = false;  
    // Opening database
    int exit = 0;
    sqlite3* DB;
    exit = sqlite3_open("languages/langhandler.db", &DB);
    if (!exit)
        cout << "Database \"langhandler.db\" opened successfully.\n";
    else
        cerr << "Error opening database!\n";
    // Keyboard for /language command
    InlineKeyboardMarkup::Ptr keyboard(new InlineKeyboardMarkup);
    vector<InlineKeyboardButton::Ptr> row0;
    InlineKeyboardButton::Ptr rusButton(new InlineKeyboardButton);
    rusButton->text = "🇷🇺 Русский";
    rusButton->callbackData = "rus";
    InlineKeyboardButton::Ptr engButton(new InlineKeyboardButton);
    engButton->text = "🇬🇧 English";
    engButton->callbackData = "eng";
    row0.push_back(engButton);
    row0.push_back(rusButton);
    keyboard->inlineKeyboard.push_back(row0);
    //
    // -------------------- On Command --------------------
    //
    bot.getEvents().onCommand("start", [&bot, &admin_chat_id, &admin, &DB](Message::Ptr message) { // Command /start
        string resp; 
        if (message->from->username == admin && message->chat->id == admin_chat_id) 
            resp = get_msg("admin_start", DB, message);
        else 
            resp = get_msg("start", DB, message);
        bot.getApi().sendMessage(message->chat->id, resp);
    });

    bot.getEvents().onCommand("convert", [&bot, &DB, &Tr](Message::Ptr message) { // Command /convert
        if (Tr.is_trusted(message->from->username) || !Tr.trusted_list()) {
            if (message->replyToMessage != nullptr && message->replyToMessage->voice != nullptr) {
                printf("\n---------- Convert used ----------\n" // print voice file data
                        "Bot got replied voice message.\n"
                        "Voice data is: [ %s, %s, %i, %s, %li ]\n", 
                        message->replyToMessage->voice->fileId.c_str(), 
                        message->replyToMessage->voice->fileUniqueId.c_str(), 
                        message->replyToMessage->voice->duration, 
                        message->replyToMessage->voice->mimeType.c_str(), 
                        message->replyToMessage->voice->fileSize);
                File::Ptr file_path = bot.getApi().getFile(message->replyToMessage->voice->fileId); // Getting file path
                string file_url = "https://api.telegram.org/file/bot" + token + "/" + file_path->filePath; // Generating link to the voice msg file
                cout << file_url << "\n"; // Output the link
                get_voice(file_url); // Downloading file

                bool is_recognized = true;
                string text;
                array<char, 128> buffer;
                FILE* pipe;

                sqlite3_exec(DB, ("SELECT * FROM chats WHERE chat_id = " + to_string(message->chat->id)).c_str(), callback_msg, 0, nullptr);

                if (message->replyToMessage->voice->duration <= 60) { // If vm is duration is less than 1 minute
                    // Transcribing voice
                    if (chatlang == "rus")
                        pipe = popen("Google-speech-api/.build/transcribe --bitrate 48000 --language-code ru audio.oga", "r");
                    else
                        pipe = popen("Google-speech-api/.build/transcribe --bitrate 48000 audio.oga", "r");
                    // Reading output
                    if (!pipe) {
                    is_recognized = false;
                    text = get_msg("reco_fail", DB, message) + "\n";
                    }
                    else {
                        cout << "Listening...\n";
                        while (fgets(buffer.data(), 128, pipe) != nullptr) {
                            text += buffer.data();
                        }
                    }
                    int returnCode = pclose(pipe);
                    if (text == "") {
                        is_recognized = false;
                        cout << "Text is empty.\n";
                        text = get_msg("reco_fail", DB, message) + "\n";
                    }
                    if (is_recognized)
                        text = get_msg("reco", DB, message) + text;
                    cout << text;
                    cout << "Return code: " << returnCode << endl;
                    
                    bot.getApi().sendMessage(message->chat->id, text, false, message->replyToMessage->messageId); // Outputting recognized text.
                }
                else { // For convertation more than 1 minute we need to upload file to gcs. I don't want to make this...
                    cout << "Voice longer than a minute.\n";
                    bot.getApi().sendMessage(message->chat->id, "Can't transcribe voice longer than a minute. Sorry!");
                }                
            }
            else {
                bot.getApi().sendMessage(message->chat->id, get_msg("reco_fail_reply", DB, message)); // If no replyed message and message isn't a voice message
            }
        }
        else {
            bot.getApi().sendMessage(message->chat->id, get_msg("not_trusted", DB, message)); // If user not in trusted list
        }
    });

    bot.getEvents().onCommand("language", [&bot, &keyboard, &DB](Message::Ptr message) { // Command /language
        bot.getApi().sendMessage(message->chat->id, get_msg("language", DB, message), false, 0, keyboard);
    });

    bot.getEvents().onCommand("about", [&bot, &DB](Message::Ptr message) { // Command /about
        bot.getApi().sendMessage(message->chat->id, get_msg("about", DB, message));
    });

    bot.getEvents().onCommand("addtrusted", [&bot, &msgid, &admin, &adding_trusted, &DB](Message::Ptr message) { // Admin command /addtrusted
        if (message->from->username == admin) {
            Message::Ptr bot_msg = bot.getApi().sendMessage(message->chat->id, get_msg("addtrusted", DB, message));
            adding_trusted = true;
            cout << "Waiting response for add trusted user...\n";
            msgid = bot_msg->messageId;
        }
    });

    bot.getEvents().onCommand("deltrusted", [&bot, &msgid, &admin, &deleting_trusted, &DB](Message::Ptr message) { // Admin command /deltrusted
        if (message->from->username == admin) {
            Message::Ptr bot_msg = bot.getApi().sendMessage(message->chat->id, get_msg("deltrusted", DB, message));
            deleting_trusted = true;
            cout << "Waiting response for delete trusted user...\n";
            msgid = bot_msg->messageId;
        }
    });

    bot.getEvents().onCommand("showtrusted", [&bot, &admin, &DB](Message::Ptr message) { // Admin command /showtrusted
        if (message->from->username == admin) {
            string user, users = "";
            ifstream file;
            cout << "The list of trusted users: ";
            file.open("trusted-users.txt", ifstream::in);
            while (!file.eof()) {
                file >> user;
                cout << user << " ";
                users = users + "\n@" + user;
            }
            cout << endl;
            bot.getApi().sendMessage(message->chat->id, get_msg("showtrusted", DB, message) + users);
        }
    });

    bot.getEvents().onCommand("enabletrusted", [&bot, &admin, &DB, &admin_chat_id, &Tr](Message::Ptr message) { // Admin command /enabletrusted
        if (message->from->username == admin && message->chat->id == admin_chat_id) {
            bot.getApi().sendMessage(message->chat->id, get_msg("enabletrusted", DB, message));
            cout << "Trusted list enabled!\n";
            Tr.trusted_list_update(true);
        }
    });

    bot.getEvents().onCommand("disabletrusted", [&bot, &admin, &DB, &admin_chat_id, &Tr](Message::Ptr message) { // Admin command /disabletrusted
        if (message->from->username == admin && message->chat->id == admin_chat_id) {
            bot.getApi().sendMessage(message->chat->id, get_msg("disabletrusted", DB, message));
            cout << "Trusted list disabled!\n";
            Tr.trusted_list_update(false);
        }
    });
    //
    // -------------------- On Callback Query --------------------
    //
    bot.getEvents().onCallbackQuery([&bot, &keyboard, &DB](CallbackQuery::Ptr query) { // When pressed button after using command /language
        cout << endl << query->from->username << " pressed button " << query->data << endl;
        sqlite3_exec(DB, (string("UPDATE chats set language = '") + query->data + "' where chat_id = " + to_string(query->message->chat->id)).c_str(), nullptr, 0, nullptr);
        cout << "Changed language in chat " << query->message->chat->id << " to " << query->data << endl;
        bot.getApi().editMessageText(get_msg("selected_language", DB, query->message), query->message->chat->id, query->message->messageId);
    });
    //
    // -------------------- On Any Message --------------------
    //
    bot.getEvents().onAnyMessage([&bot, &msgid, &admin, &deflang, &adding_trusted, &deleting_trusted, &DB](Message::Ptr message) { 
        time_t now = time(0); // Getting time
        char* dt = ctime(&now);
        cout << "\n---------- New message ----------\n" << dt; // Printing separation and time
        sqlite3_exec(DB, ("SELECT * FROM chats WHERE chat_id = " + to_string(message->chat->id)).c_str(), callback, 0, nullptr);
        if(!is_chat_in_db(DB, message)) { // If chat isn't in db
            cout << "Adding " << message->chat->id << " with default \"" << deflang << "\" language to the db...\n";
            sqlite3_exec(DB, (string("INSERT INTO chats VALUES(") + to_string(message->chat->id) + ", '" + deflang + "')").c_str(),nullptr , 0, nullptr);
            bot.getApi().sendMessage(message->chat->id, get_msg("lang_prefer", DB, message));
        }
        // Printing type of message in console
        cout << message->from->username; 
        if (StringTools::startsWith(message->text, "/")) 
            cout << " used command: " << message->text.c_str();
        else if (message->text != "")
            cout << " wrote: " << message->text.c_str();
        else if (message->voice)
            cout << " sent a voice message";
        else if (message->sticker)
            cout << " sent a sticker";
        else if (!message->photo.empty())
            cout << " sent a photo";
        else if (message->video)
            cout << " sent a video";
        else if (message->videoNote)
            cout << " sent a video node";
        else if (message->animation)
            cout << " sent a gif";
        else if (message->audio)
            cout << " sent a audio";
        else if (message->document)
            cout << " sent a document";

        if ((message->animation || message->audio || message->document || !message->photo.empty() || message->video || message->voice) && message->caption != "") 
            cout << " with caption: " << message->caption;
        
        cout << endl;
        // For trusted list users
        if (message->replyToMessage && message->from->username == admin) {
            if (adding_trusted && msgid == message->replyToMessage->messageId) {
                adding_trusted = false;
                string msg = message->text;

                if (StringTools::startsWith(msg, "@")) {
                    msg.erase(0,1);
                }
                cout << "Adding @" << msg << "...\n";

                ofstream users;
                users.open("trusted-users.txt", ofstream::out | ofstream::app);
                users << endl << msg;
                users.close();

                bot.getApi().sendMessage(message->chat->id, get_msg("added_trusted", DB, message) + msg);
            }
            else if (deleting_trusted && msgid == message->replyToMessage->messageId) {
                deleting_trusted = false;
                string msg = message->text;
                string user;
                vector<string> susers;
                int i = 0;

                if (StringTools::startsWith(msg, "@")) {
                    msg.erase(0,1);
                }
                cout << "Deleting @" << msg << "...\n";

                ifstream users;
                users.open("trusted-users.txt", ifstream::in);
                while(!users.eof()) {
                    users >> user;
                    if (user != msg)
                        susers.push_back(user);
                }
                users.close();

                ofstream usersus;
                usersus.open("trusted-users.txt", ofstream::out | ofstream::trunc);
                while (!susers.empty()){
                    user = susers.back();
                    susers.pop_back();
                    if (i == 0) 
                        usersus << user;
                    else
                        usersus << endl << user;
                    i++;
                }
                usersus.close();

                bot.getApi().sendMessage(message->chat->id, get_msg("deleted_trusted", DB, message) + msg);
            }
            else if (msgid != message->replyToMessage->messageId && (adding_trusted || deleting_trusted)) {
                bot.getApi().sendMessage(message->chat->id, get_msg("trusted_reply_fail", DB, message));
            }
        }
        else if ((adding_trusted || deleting_trusted) && message->from->username == admin && !message->replyToMessage) {
            bot.getApi().sendMessage(message->chat->id, get_msg("trusted_fail", DB, message));
        }
    });
    //
    // -------------------- Long poll --------------------
    //
    try {
        printf("Logged on with username: %s\n", bot.getApi().getMe()->username.c_str());
        TgLongPoll longPoll(bot);
        while (true) {
            longPoll.start();
        }
    } catch (TgException& e) {
        printf("error: %s\n", e.what());
    }
    // Exiting program
    sqlite3_close(DB);
    return 0;
}