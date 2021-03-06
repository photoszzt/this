//
// async_client.cpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2010 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <vector>
#include <chrono>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/asio/ssl.hpp>
#include <thread>
#include <fstream>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/insert_linebreaks.hpp>
#include <ctime>
#include "json.hpp"

using namespace boost::archive::iterators;
using boost::asio::ip::tcp;
using json = nlohmann::json;

class client
{
public:
    std::stringstream resultss;
    client(boost::asio::io_service& io_service,
           const std::string& server, const std::string& path, const std::string& binaryImgStr)
            : resolver_(io_service),
              ctx(io_service, boost::asio::ssl::context::method::tlsv12_client),
              socket_(io_service, ctx)
    {
        SSL_set_tlsext_host_name(socket_.native_handle(),server.c_str());
        ctx.set_default_verify_paths();
        /*ctx.set_options(boost::asio::ssl::context::default_workarounds
                        | boost::asio::ssl::context::no_sslv2
                        | boost::asio::ssl::context::no_sslv3
                        | boost::asio::ssl::context::no_tlsv1
                        | boost::asio::ssl::context::no_tlsv1_1);*/
        //socket_.set_verify_mode(boost::asio::ssl::verify_none);
        //ctx.load_verify_file("ca.pem");
        std::string b64ImgStr = base64_encode(reinterpret_cast<const unsigned char*>(binaryImgStr.c_str()), binaryImgStr.length());
        // Form the request. We specify the "Connection: close" header so that the
        // server will close the socket after transmitting the response. This will
        // allow us to treat all data up until the EOF as the content.
        std::ostream request_stream(&request_);
        request_stream << "POST " << path << " HTTP/1.1\r\n";
        request_stream << "Host: " << server << "\r\n";
        request_stream << "content-type: application/json;charset=UTF-8\r\n";
        std::string bodyStartStr = "{\"httpMethod\":\"POST\",\"pathWithQueryString\":\"/mxnet-test-dev-hello\",\"body\":\"{\\\"b64Img\\\": \\\"";
        std::string bodyEndStr = "\\\"}\",\"headers\":{},\"stageVariables\":{},\"withAuthorization\":false}\r\n";
        request_stream << "content-length: " << std::to_string(bodyStartStr.length() + b64ImgStr.length() + bodyEndStr.length()) << "\r\n";
        request_stream << "Connection: close\r\n\r\n";
        request_stream << bodyStartStr << b64ImgStr << bodyEndStr;
        
        //request_stream << "Accept: */*\r\n";

        // Start an asynchronous resolve to translate the server and service names
        // into a list of endpoints.
        tcp::resolver::query query(server, "https");

        resolver_.async_resolve(query,
                                boost::bind(&client::handle_resolve, this,
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::iterator));
    }

    // void check_finished() {
    //     if (!finished) {
    //         std::cout << "something wrong, lambda did not complete" << std::endl;
    //     }
    // }
    ~client() {
       
    }
private:


    // http://renenyffenegger.ch/notes/development/Base64/Encoding-and-decoding-base-64-with-cpp
    const std::string base64_chars =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                    "abcdefghijklmnopqrstuvwxyz"
                    "0123456789+/";

    std::string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len) {
        std::string ret;
        int i = 0;
        int j = 0;
        unsigned char char_array_3[3];
        unsigned char char_array_4[4];

        while (in_len--) {
            char_array_3[i++] = *(bytes_to_encode++);
            if (i == 3) {
                char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
                char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
                char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
                char_array_4[3] = char_array_3[2] & 0x3f;

                for(i = 0; (i <4) ; i++)
                    ret += base64_chars[char_array_4[i]];
                i = 0;
            }
        }

        if (i)
        {
            for(j = i; j < 3; j++)
                char_array_3[j] = '\0';

            char_array_4[0] = ( char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

            for (j = 0; (j < i + 1); j++)
                ret += base64_chars[char_array_4[j]];

            while((i++ < 3))
                ret += '=';

        }

        return ret;

    }

    //typedef transform_width< binary_from_base64<remove_whitespace<std::string::const_iterator> >, 8, 6 > it_binary_t;
/*
    std::string b64Encode(const std::string& binaryStr)
    {
        typedef insert_linebreaks<base64_from_binary<transform_width<std::string::const_iterator,6,8> >, 72 > it_base64_t;
        unsigned int writePaddChars = (3-binaryStr.length()%3)%3;
        std::string base64(it_base64_t(binaryStr.begin()),it_base64_t(binaryStr.end()));
        base64.append(writePaddChars,'=');
        return base64;
    }
*/
    void handle_resolve(const boost::system::error_code& err,
                        tcp::resolver::iterator endpoint_iterator)
    {
        if (!err)
        {
            // std::cout << "Resolve OK" << "\n";
            socket_.set_verify_mode(boost::asio::ssl::verify_peer);
            socket_.set_verify_callback(
                    boost::bind(&client::verify_certificate, this, _1, _2));

            boost::asio::async_connect(socket_.lowest_layer(), endpoint_iterator,
                                       boost::bind(&client::handle_connect, this,
                                                   boost::asio::placeholders::error));
        }
        else
        {
            std::cout << "Error resolve: " << err.message() << "\n";
        }
    }

    bool verify_certificate(bool preverified,
                            boost::asio::ssl::verify_context& ctx)
    {
        // The verify callback can be used to check whether the certificate that is
        // being presented is valid for the peer. For example, RFC 2818 describes
        // the steps involved in doing this for HTTPS. Consult the OpenSSL
        // documentation for more details. Note that the callback is called once
        // for each certificate in the certificate chain, starting from the root
        // certificate authority.

        // In this example we will simply print the certificate's subject name.
        char subject_name[256];
        X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
        X509_NAME_oneline(X509_get_subject_name(cert), subject_name, 256);
        // std::cout << "Verifying " << subject_name << "\n";

        return preverified;
    }

    void handle_connect(const boost::system::error_code& err)
    {
        if (!err)
        {
            // std::cout << "Connect OK " << "\n";
            // socket_.lowest_layer().set_option(tcp::no_delay(true));
            socket_.async_handshake(boost::asio::ssl::stream_base::client,
                                    boost::bind(&client::handle_handshake, this,
                                                boost::asio::placeholders::error));
        }
        else
        {
            std::cout << "Connect failed: " << err.message() << "\n";
        }
    }
  
    void handle_handshake(const boost::system::error_code& error)
    {
        if (!error)
        {
            // std::cout << "Handshake OK " << "\n";
            // std::cout << "Request: " << "\n";
            // const char* header=boost::asio::buffer_cast<const char*>(request_.data());
            // std::cout << header << "\n";

            // The handshake was successful. Send the request.
            boost::asio::async_write(socket_, request_,
                                     boost::bind(&client::handle_write_request, this,
                                                 boost::asio::placeholders::error));
        }
        else
        {
            std::cout << "Handshake failed: " << error.message() << "\n";
        }
    }

    void handle_write_request(const boost::system::error_code& err)
    {
        if (!err)
        {
            // Read the response status line. The response_ streambuf will
            // automatically grow to accommodate the entire line. The growth may be
            // limited by passing a maximum size to the streambuf constructor.
            // std::cout << "finished transfer data" << std::endl;
            boost::asio::async_read_until(socket_, response_, "\r\n",
                                          boost::bind(&client::handle_read_status_line, this,
                                                      boost::asio::placeholders::error));
        }
        else
        {
            std::cout << "Error write req: " << err.message() << "\n";
        }
    }

    void handle_read_status_line(const boost::system::error_code& err)
    {
        if (!err)
        {
            // Check that response is OK.
            std::istream response_stream(&response_);
            std::string http_version;
            response_stream >> http_version;
            unsigned int status_code;
            response_stream >> status_code;
            std::string status_message;
            std::getline(response_stream, status_message);
            if (!response_stream || http_version.substr(0, 5) != "HTTP/")
            {
                std::cout << "Invalid response\n";
                return;
            }
            if (status_code != 200)
            {
                std::cout << "Response returned with status code ";
                std::cout << status_code << "\n";
                return;
            }
            // std::cout << "Status code: " << status_code << "\n";

            // Read the response headers, which are terminated by a blank line.
            boost::asio::async_read_until(socket_, response_, "\r\n\r\n",
                                          boost::bind(&client::handle_read_headers, this,
                                                      boost::asio::placeholders::error));
        }
        else
        {
            std::cout << "Error: " << err.message() << "\n";
        }
    }

    void handle_read_headers(const boost::system::error_code& err)
    {
        if (!err)
        {
            // Process the response headers.
            std::istream response_stream(&response_);
            std::string header;
            while (std::getline(response_stream, header) && header != "\r") {
                // std::cout << header << "\n";
            }
            // std::cout << "\n";

            // Write whatever content we already have to output.
            if (response_.size() > 0)
                resultss << &response_;

            // Start reading remaining data until EOF.
            boost::asio::async_read(socket_, response_,
                                    boost::asio::transfer_at_least(1),
                                    boost::bind(&client::handle_read_content, this,
                                                boost::asio::placeholders::error));
        }
        else
        {
            std::cout << "Error: " << err << "\n";
        }
    }

    void handle_read_content(const boost::system::error_code& err)
    {
        if (!err)
        {
            // std::cout << "reading the content..." << std::endl;
            // Write all of the data that has been read so far.
            // std::cout << &response_;
            resultss << &response_;
            // Continue reading remaining data until EOF.
            boost::asio::async_read(socket_, response_,
                                    boost::asio::transfer_at_least(1),
                                    boost::bind(&client::handle_read_content, this,
                                                boost::asio::placeholders::error));
        }
        else if (err != boost::asio::error::eof)
        {
            std::cout << "Error: " << err << "\n";
        } 
    }

    tcp::resolver resolver_;
    // https://stackoverflow.com/questions/40036854/creating-a-https-request-using-boost-asio-and-openssl
    // http://www.boost.org/doc/libs/1_65_1/doc/html/boost_asio/overview/ssl.html
    boost::asio::ssl::context ctx;
    boost::asio::ssl::stream<tcp::socket> socket_;
    boost::asio::streambuf request_;
    boost::asio::streambuf response_;
    // bool finished = false;
};

void execute_frame(const std::string& server, const std::string& path, 
                    const std::string& binaryImgStr) {

    boost::asio::io_service io_service;
    client c(io_service, server, path, binaryImgStr);
    io_service.run();

    // c.check_finished();
    std::cout << "The response is: \n";
    std::cout << c.resultss.str();
    json js = json::parse(c.resultss.str());
    std::string bodystr = js["body"];
    // std::cout << "\n The body content is: \n";
    // std::cout << bodystr << std::endl;

    json bodyjs = json::parse(bodystr);
    std::cout << "\nHighest possible class is: ";
    // std::cout << bodyjs["0"] << "\n";
    json objs = bodyjs["0"];
    std::string ret_class = objs.begin().key();
    std::cout << ret_class << "\n";

}

std::string parse_result(const std::string& resultstr) {

    json js = json::parse(resultstr);
    std::string bodystr = js["body"];
    // std::cout << "\n The body content is: \n";
    // std::cout << bodystr << std::endl;

    json bodyjs = json::parse(bodystr);
    // std::cout << "\nHighest possible class is: ";
    // std::cout << bodyjs["0"] << "\n";
    json objs = bodyjs["0"];
    std::string ret_class = objs.begin().key();
    // std::cout << ret_class << "\n";
    return ret_class;
}

// asynchronously execute a sequence of frames, duplicate the call
void async_execute_frames(const std::string& server, const std::string& path, 
                    const std::string& binaryImgStr, 
                    const std::string& binaryImgStr2, const int iters) {
    int maxnum = iters * 2;
    std::vector<client *> frames;
    std::vector<std::string> results;
    boost::asio::io_service io_service;
    int err_cnt = 0;

    // async launch a vector of frames
    for (int i = 0; i < maxnum; ++i) {
        client *pc;
        if (i < maxnum / 2) {
            pc = new client(io_service, server, path, binaryImgStr);
        } else {
            pc = new client(io_service, server, path, binaryImgStr2);
        }
        io_service.poll(); // run the ready handlers.

        frames.push_back(pc);
        std::cout << "launch lambda: " << i << std::endl;
        // std::this_thread::sleep_for(std::chrono::milliseconds(10));

    }
    
    io_service.run(); // this is a blocking operation. Wait all finished.
    io_service.reset();

    // then sync wait for results
    for (int i = 0; i < maxnum; ++i) {
        // wait for the execution
        // frames[i]->check_finished();
        std::cout << "lambda " << i << " finished" << std::endl;
        // std::cout << "result string : " << i << "\t";
        // std::cout << frames[i]->resultss.str() << std::endl;
        std::string result;
        if (frames[i]->resultss.str().size() > 0) {
            result = parse_result(frames[i]->resultss.str());
            results.push_back(result);
        }
        else {
            std::cout << "error happened, retry" << std::endl;
            err_cnt++;
            delete frames[i];
            frames[i] = NULL;
            client *pc;
            if (i < maxnum / 2) {
                pc = new client(io_service, server, path, binaryImgStr);
            } else {
                pc = new client(io_service, server, path, binaryImgStr2);
            }
            io_service.run();
            io_service.reset();
            std::cout << "retry finished" << std::endl;
            if (pc->resultss.str().size() > 0) {
                result = parse_result(pc->resultss.str());
                results.push_back(result);
            } else {
                results.push_back("nullstring");
            }
            delete pc;
            pc = NULL;
        }
        
        
    }
    
    // then display the results
    for (int i = 0; i < maxnum; ++i) {
        std::cout << "result : " << i << "\t";
        std::cout << results[i] << std::endl;
    }

    std::cout << "Error rate is: " << (double)err_cnt / maxnum << std::endl;
    for (int i = 0; i < maxnum; ++i) {
        if (frames[i] != NULL)
            delete frames[i];
        frames[i] = NULL;
    }
    return;
}

// sequentially execute frames, one after another
void sync_execute_frames(const std::string& server, const std::string& path, 
                    const std::string& binaryImgStr, const int iters) {
    int maxnum = iters;
    std::vector<std::string> results;

    // async launch a vector of frames
    for (int i = 0; i < maxnum; ++i) {
        boost::asio::io_service io_service;
        client *pc = new client(io_service, server, path, binaryImgStr);
        std::cout << "launch lambda: " << i << std::endl;
        io_service.run();
        // pc->check_finished();
        std::cout << "lambda " << i << " finished" << std::endl;
        std::string result = parse_result(pc->resultss.str());
        results.push_back(result);
        delete pc;
    }
    
    
    // then display the results
    for (int i = 0; i < maxnum; ++i) {
        std::cout << "result : " << i << "\t";
        std::cout << results[i] << std::endl;
    }
    return;
}

int main(int argc, char* argv[])
{
    try
    {
        if (argc != 6)
        {
            std::cout << "Usage: kernel_dev <server> <path> <fileContainingPostBody> <file2> <iters>\n";
            std::cout << "Example:\n";
            std::cout << "  async_client www.boost.org /LICENSE_1_0.txt /file.txt 100\n";
            return 1;
        }

        std::ifstream b64File(argv[3]);
        std::ifstream b64File2(argv[4]);

        int iters = atoi(argv[5]);
        std::stringstream b64ImgStream;
        std::stringstream b64ImgStream2;

        assert(iters > 0);

        b64ImgStream << b64File.rdbuf();
        b64ImgStream2 << b64File2.rdbuf();

        std::string binaryImgStr = b64ImgStream.str();
        std::string binaryImgStr2 = b64ImgStream2.str();
        //client c(io_service, "www.deelay.me", "/1000/hmpg.net");

        struct timeval start, stop;
        double duration;

        std::cout << "single run: \n";
        gettimeofday(&start, NULL);
        execute_frame(argv[1], argv[2], binaryImgStr);
        execute_frame(argv[1], argv[2], binaryImgStr2);
        gettimeofday(&stop, NULL);
        duration = (stop.tv_sec - start.tv_sec) * 1000.0 +
                   (stop.tv_usec - start.tv_usec) / 1000.0;
        std::cout << "single run time: " << duration << " ms\n";

        // std::cout << "\nsynchronous multiple runs: "<< iters << " times \n";
        // gettimeofday(&start, NULL);
        // sync_execute_frames(argv[1], argv[2], binaryImgStr, iters);
        // sync_execute_frames(argv[1], argv[2], binaryImgStr2, iters);
        // gettimeofday(&stop, NULL);
        // duration = (stop.tv_sec - start.tv_sec) * 1000.0 +
        //            (stop.tv_usec - start.tv_usec) / 1000.0;
        // std::cout << "Sync execution run time: " << duration << " ms\n";

        std::cout << "\nasynchronous multiple runs: " << iters << " times \n";
        gettimeofday(&start, NULL);
        async_execute_frames(argv[1], argv[2], binaryImgStr, 
                             binaryImgStr2, iters);
        gettimeofday(&stop, NULL);
        duration = (stop.tv_sec - start.tv_sec) * 1000.0 +
                   (stop.tv_usec - start.tv_usec) / 1000.0;
        std::cout << "Async execution run time: " << duration << " ms\n";

        
    }
    catch (std::exception& e)
    {
        std::cout << "Exception: " << e.what() << "\n";
    }

    return 0;
}
