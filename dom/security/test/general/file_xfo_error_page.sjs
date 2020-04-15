"use strict";

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);
  response.setHeader("x-frame-options", "deny", false);
  response.write("<html>xfo test loaded</html>");
}
