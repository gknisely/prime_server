#ifndef __HTTP_PROTOCOL_HPP__
#define __HTTP_PROTOCOL_HPP__

#include <prime_server/prime_server.hpp>
#include <prime_server/zmq_helpers.hpp>

#include <unordered_map>
#include <list>
#include <cstdint>
#include <string>
#include <limits>

namespace prime_server {

  class http_client_t : public client_t {
   public:
    http_client_t(zmq::context_t& context, const std::string& server_endpoint, const request_function_t& request_function,
      const collect_function_t& collect_function, size_t batch_size = 8912);
   protected:
    virtual size_t stream_responses(const void* message, size_t size, bool& more);
    void reset();

    bool parse_headers;
    bool parse_length;
    bool parse_body;
    std::string content_length;
    size_t length;

    std::string::const_iterator double_return_itr;
    std::string::const_iterator content_length_itr;

    std::string buffer;
  };

  using headers_t = std::unordered_map<std::string, std::string>;
  using query_t = std::unordered_map<std::string, std::list<std::string> >;
  enum method_t { OPTIONS, GET, HEAD, POST, PUT, DELETE, TRACE, CONNECT };
  const std::unordered_map<std::string, method_t> STRING_TO_METHOD {
    {"OPTIONS", method_t::OPTIONS}, {"GET", method_t::GET}, {"HEAD", method_t::HEAD}, {"POST", method_t::POST},
    {"PUT", method_t::PUT}, {"DELETE", method_t::DELETE}, {"TRACE", method_t::TRACE}, {"CONNECT", method_t::CONNECT}
  };
  const std::unordered_map<method_t, std::string, std::hash<int> > METHOD_TO_STRING{
    {method_t::OPTIONS, "OPTIONS"}, {method_t::GET, "GET"}, {method_t::HEAD, "HEAD"}, {method_t::POST, "POST"},
    {method_t::PUT, "PUT"}, {method_t::DELETE, "DELETE"}, {method_t::TRACE, "TRACE"}, {method_t::CONNECT, "CONNECT"}
  };
  const std::unordered_map<std::string, bool> SUPPORTED_VERSIONS {
    {"HTTP/1.0", true}, {"HTTP/1.1", true}
  };

  struct http_entity_t {
    std::string version;
    headers_t headers;
    std::string body;
    http_entity_t(const std::string& version, const headers_t& headers, const std::string& body);

    virtual ~http_entity_t();
    virtual std::string to_string() const = 0;
   protected:
    enum state_t { METHOD, MESSAGE, CODE, PATH, VERSION, HEADERS, BODY, CHUNKS };
    virtual void flush_stream(const state_t state);

    //state for streaming parsing
    const char *cursor, *end, *delimiter;
    std::string partial_buffer;
    size_t partial_length;
    state_t state;
    size_t body_length;
    size_t consumed;

    bool consume_until();
  };

  class http_server_t;
  struct http_request_t : public http_entity_t {
    friend http_server_t;
   public:
    method_t method;
    std::string path;
    query_t query;

    virtual ~http_request_t();
    http_request_t();
    http_request_t(const method_t& method, const std::string& path, const std::string& body = "", const query_t& query = query_t{},
                   const headers_t& headers = headers_t{}, const std::string& version = "HTTP/1.1");

    struct info_t {
      uint64_t id                         :32; //the request id
      uint64_t version                    :3;  //protocol specific space for versioning info
      uint64_t connection_keep_alive      :1;  //header present or not
      uint64_t connection_close           :1;  //header present or not
      uint64_t do_not_track               :1;  //header present or not
      uint64_t response_code              :10; //what the response code was set to when sent back to the client
      uint64_t spare                      :16; //unused information
    };

    info_t to_info(uint64_t id) const;
    virtual std::string to_string() const;
    static std::string to_string(const method_t& method, const std::string& path, const std::string& body = "", const query_t& query = query_t{},
                                 const headers_t& headers = headers_t{}, const std::string& version = "HTTP/1.1");
    static http_request_t from_string(const char* start, size_t length);
    static query_t split_path_query(std::string& path);
    std::list<http_request_t> from_stream(const char* start, size_t length, size_t max_size = std::numeric_limits<size_t>::max());
    virtual void flush_stream();
    size_t size() const;

   protected:
    std::string log_line;
  };

  //TODO: let this subclass exception and make 'message' be the 'what'
  //then the caught exceptions that we want to actually return to the client
  //can be handled more easily
  struct http_response_t : public http_entity_t {
   public:
    uint16_t code;
    std::string message;

    virtual ~http_response_t();
    http_response_t();
    http_response_t(unsigned code, const std::string& message, const std::string& body = "", const headers_t& headers = headers_t{},
                    const std::string& version = "HTTP/1.1");
    void from_info(http_request_t::info_t& info);
    virtual std::string to_string() const;
    static http_response_t from_string(const char* start, size_t length);
    std::list<http_response_t> from_stream(const char* start, size_t length);
    static std::string generic(unsigned code, const std::string message, const headers_t& headers = headers_t{}, const std::string& body = "",
                               const std::string& version = "HTTP/1.1");
    virtual void flush_stream();

   protected:
    std::string log_line;
  };

  class http_server_t : public server_t<http_request_t, http_request_t::info_t> {
   public:
    http_server_t(zmq::context_t& context, const std::string& client_endpoint, const std::string& proxy_endpoint,
                  const std::string& result_endpoint, bool log = false, size_t max_request_size = DEFAULT_MAX_REQUEST_SIZE);
    virtual ~http_server_t();
   protected:
    virtual bool enqueue(const void* message, size_t size, const std::string& requester, http_request_t& request);
    virtual void dequeue(const http_request_t::info_t& request_info, size_t length);
   protected:
    uint64_t request_id;
  };

}

#endif //__HTTP_PROTOCOL_HPP__
