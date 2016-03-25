/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
function handleRequest(request, response) {
  try {
    var cookie = request.getHeader("Cookie");
  } catch (e) {
    cookie = "EMPTY_COOKIE";
  }

  // avoid confusing cache behaviors.
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-type", "text/plain", false);
  response.setStatusLine(request.httpVersion, "200", "OK");
  response.write(cookie);
}
