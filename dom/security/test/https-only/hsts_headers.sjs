/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(request, response) {
  if (request.queryString === "reset") {
    // Reset the HSTS policy, prevent influencing other tests
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Strict-Transport-Security", "max-age=0");
    response.write("Resetting HSTS");
    return;
  }
  let hstsHeader = "max-age=60";
  response.setHeader("Strict-Transport-Security", hstsHeader);
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);
  // Set header for csp upgrade
  response.setHeader(
    "Content-Security-Policy",
    "upgrade-insecure-requests",
    false
  );
  response.setStatusLine(request.httpVersion, 200);
  response.write("<!DOCTYPE html><html><body><h1>Ok!</h1></body></html>");
}
