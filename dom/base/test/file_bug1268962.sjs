// Test server for bug 1268962
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

const SAME_ORIGIN =
  "http://mochi.test:8888/tests/dom/base/test/file_bug1268962.sjs";
const CROSS_ORIGIN =
  "http://example.com/tests/dom/base/test/file_bug1268962.sjs";

function handleRequest(request, response) {
  const queryMap = new URLSearchParams(request.queryString);

  // Check redirection before everything else.
  if (queryMap.has("redirect")) {
    let redirect = queryMap.get("redirect");
    let location;
    if (redirect == "sameorigin") {
      location = SAME_ORIGIN;
    } else if (redirect == "crossorigin") {
      location = CROSS_ORIGIN;
    }

    if (location) {
      // Use HTTP 302 redirection.
      response.setStatusLine("1.1", 302, HTTPStatus.get(302));

      // Forward query strings except the redirect option.
      queryMap.delete("redirect");
      response.setHeader("Location", location + "?" + queryMap.toString());

      return;
    }
  }

  if (queryMap.has("statusCode")) {
    let statusCode = parseInt(queryMap.get("statusCode"));
    let statusText = HTTPStatus.get(statusCode);
    response.setStatusLine("1.1", statusCode, statusText);
  }
  if (queryMap.has("cacheControl")) {
    let cacheControl = queryMap.get("cacheControl");
    response.setHeader("Cache-Control", cacheControl);
  }
  if (queryMap.has("allowOrigin")) {
    let allowOrigin = queryMap.get("allowOrigin");
    response.setHeader("Access-Control-Allow-Origin", allowOrigin);
  }
}
