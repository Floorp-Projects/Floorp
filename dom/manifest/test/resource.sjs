/* Generic responder that composes a response from
 * the query string of a request.
 *
 * It reserves some special prop names:
 *  - body: get's used as the response body
 *  - statusCode: override the 200 OK response code
 *    (response text is set automatically)
 *
 * Any property names it doesn't know about get converted into
 * HTTP headers.
 *
 * For example:
 *  http://test/resource.sjs?Content-Type=text/html&body=<h1>hello</h1>&Hello=hi
 *
 * Outputs:
 * HTTP/1.1 200 OK
 * Content-Type: text/html
 * Hello: hi
 * <h1>hello</h1>
 */
//global handleRequest
"use strict";

const HTTPStatus = new Map([
  [100, "Continue"],
  [101, "Switching Protocol"],
  [200, "OK"],
  [201, "Created"],
  [202, "Accepted"],
  [203, "Non-Authoritative Information"],
  [204, "No Content"],
  [205, "Reset Content"],
  [206, "Partial Content"],
  [300, "Multiple Choice"],
  [301, "Moved Permanently"],
  [302, "Found"],
  [303, "See Other"],
  [304, "Not Modified"],
  [305, "Use Proxy"],
  [306, "unused"],
  [307, "Temporary Redirect"],
  [308, "Permanent Redirect"],
  [400, "Bad Request"],
  [401, "Unauthorized"],
  [402, "Payment Required"],
  [403, "Forbidden"],
  [404, "Not Found"],
  [405, "Method Not Allowed"],
  [406, "Not Acceptable"],
  [407, "Proxy Authentication Required"],
  [408, "Request Timeout"],
  [409, "Conflict"],
  [410, "Gone"],
  [411, "Length Required"],
  [412, "Precondition Failed"],
  [413, "Request Entity Too Large"],
  [414, "Request-URI Too Long"],
  [415, "Unsupported Media Type"],
  [416, "Requested Range Not Satisfiable"],
  [417, "Expectation Failed"],
  [500, "Internal Server Error"],
  [501, "Not Implemented"],
  [502, "Bad Gateway"],
  [503, "Service Unavailable"],
  [504, "Gateway Timeout"],
  [505, "HTTP Version Not Supported"],
]);

function handleRequest(request, response) {
  const queryMap = new URLSearchParams(request.queryString);
  if (queryMap.has("statusCode")) {
    let statusCode = parseInt(queryMap.get("statusCode"));
    let statusText = HTTPStatus.get(statusCode);
    queryMap.delete("statusCode");
    response.setStatusLine("1.1", statusCode, statusText);
  }
  if (queryMap.has("body")) {
    let body = queryMap.get("body") || "";
    queryMap.delete("body");
    response.write(decodeURIComponent(body));
  }
  for (let [key, value] of queryMap.entries()) {
    response.setHeader(key, value);
  }
}
