"use strict";

function handleRequest(request, response) {
  // sandbox without allow-same-origin forces an opaque origin (null principal).
  response.setHeader("Content-Security-Policy", "sandbox", false);
  response.write("This page has Content-Security-Policy: sandbox");
}
