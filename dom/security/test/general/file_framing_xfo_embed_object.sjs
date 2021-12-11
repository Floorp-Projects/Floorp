"use strict";

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("x-frame-options", "deny", false);
  response.write("<html>doc with x-frame-options: deny</html>");
}
