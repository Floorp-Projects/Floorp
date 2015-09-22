/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  response.setHeader("Pragma", "no-cache");
  response.setHeader("Expires", "0");

  response.setHeader("Access-Control-Allow-Origin", "*", false);

  if (request.scheme === "http") {
    response.setStatusLine(request.httpVersion, 302, "Found");
    response.setHeader("Location", "https://" + request.host + request.path);
  } else {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write("Page was accessed over HTTPS!");
  }

}
