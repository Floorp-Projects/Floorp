/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);
  if (request.scheme === "https") {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "image/svg+xml");
    response.write(
      `<svg version="1.1" width="100" height="40" xmlns="http://www.w3.org/2000/svg"><text x="20" y="20">HTTPS</text></svg>`
    );
    return;
  }
  if (request.scheme === "http") {
    response.setStatusLine(request.httpVersion, 400, "Bad Request");
  }
}
