"use strict";

function handleRequest(request, response) {

  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);

  let query = request.queryString;
  if (query === "xfo") {
    response.setHeader("x-frame-options", "deny", false);
    response.write("<html>xfo test loaded</html>");
    return;
  }

  if (query === "csp") {
    response.setHeader("content-security-policy", "frame-ancestors 'none'", false);
    response.write("<html>csp test loaded</html>");
    return;
  }

  // we should never get here, but just in case
  // return something unexpected
  response.write("do'h");
}
