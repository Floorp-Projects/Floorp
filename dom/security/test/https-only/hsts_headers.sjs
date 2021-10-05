/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(request, response) {
  let hstsHeader = "max-age=300";
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
