/* vim: set ft=javascript: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
function handleRequest(request, response) {
  response.setHeader("Content-type", "text/html", false);
  response.setStatusLine(request.httpVersion, "302", "Found");
  response.setHeader("Location", request.queryString, false);
  response.write("<!DOCTYPE html><html><body>Look away!</body></html>");
}
