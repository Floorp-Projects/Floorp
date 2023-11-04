"use strict";

async function handleRequest(request, response) {
  if (request.method !== "POST") {
    throw new Error("Expected a post request");
  } else {
    let params = new URLSearchParams(request.queryString);
    let redirect = params.get("redirectTo");

    response.setStatusLine(request.httpVersion, 302, "Moved Temporarily");
    response.setHeader("Location", redirect);
  }
}
