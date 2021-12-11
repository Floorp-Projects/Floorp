/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let gResponses = [];

function handleRequest(request, response) {
  response.seizePower();
  response.write("HTTP/1.1 200 OK\r\n");
  response.write("Content-Type: text/plain; charset=utf-8\r\n");
  response.write("Cache-Control: no-cache, must-revalidate\r\n");
  response.write("\r\n");

  // Keep the response alive indefinitely.
  gResponses.push(response);
}
