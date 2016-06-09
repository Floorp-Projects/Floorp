/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  response.setHeader("Pragma", "no-cache");
  response.setHeader("Expires", "0");

  if (request.queryString === "reset") {
    // Reset the HSTS policy, prevent influencing other tests
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Strict-Transport-Security", "max-age=0");
    response.write("Resetting HSTS");
  } else if (request.scheme === "http") {
    response.setStatusLine(request.httpVersion, 302, "Found");
    response.setHeader("Location", "https://" + request.host + request.path);
  } else {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Strict-Transport-Security", "max-age=100");
    response.write("Page was accessed over HTTPS!");
  }
}
