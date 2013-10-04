/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "application/javascript; charset=utf-8", false);
  response.write([
    "window.setInterval(function bacon() {",
    "  var x = '" + Math.random() + "';",
    "}, 0);"].join("\n"));
}
