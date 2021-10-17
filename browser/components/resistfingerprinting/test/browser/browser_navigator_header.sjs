"use strict";

function handleRequest(request, response) {
  response.setHeader("Content-Type", "text/plain; charset=UTF-8", false);

  if (request.hasHeader("user-agent")) {
    response.write(request.getHeader("user-agent"));
  } else {
    response.write("no user agent header");
  }
}