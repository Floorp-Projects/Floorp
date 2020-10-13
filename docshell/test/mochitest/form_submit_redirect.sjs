"use strict";

async function handleRequest(request, response) {
  if (request.method !== "POST") {
    message = "bad";
  } else {
    response.setStatusLine(request.httpVersion, 302, "Moved Temporarily");
    response.setHeader("Location", request.getHeader("referer"));
  }
}
