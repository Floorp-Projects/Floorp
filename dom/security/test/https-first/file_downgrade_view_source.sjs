"use strict";

function handleRequest(request, response) {
  // avoid confusing cache behaviour
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);

  let query = request.queryString.split("&");
  let scheme = request.scheme;

  if (scheme === "https") {
    if (query.includes("downgrade")) {
      response.setStatusLine("1.1", 400, "Bad Request");
      response.write("Bad Request\n");
      return;
    }
    if (query.includes("upgrade")) {
      response.write("view-source:https://");
      return;
    }
  }

  if (scheme === "http" && query.includes("downgrade")) {
    response.write("view-source:http://");
    return;
  }

  // We should arrive here when the redirection was downraded successful
  response.write("unexpected scheme and query given");
}
