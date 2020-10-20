/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* exported handleRequest */

"use strict";

// Simple server that writes a text response displaying the value of the
// cache-control header:
// - if the header is missing, the text will be `cache-control:`
// - if the header is available, the text will be `cache-control:${value}`
function handleRequest(request, response) {
  response.setHeader("Content-Type", "text/plain; charset=utf-8", false);
  if (request.hasHeader("cache-control")) {
    response.write(`cache-control:${request.getHeader("cache-control")}`);
  } else {
    response.write(`cache-control:`);
  }
}
