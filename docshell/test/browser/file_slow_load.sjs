"use strict";

function handleRequest(request, response) {
  response.processAsync();
  response.setHeader("Content-Type", "text/html");
  response.write("<!doctype html>Loading... ");
  // We don't block on this, so it's fine to never finish the response.
}
