/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function handleRequest(request, response) {
  // Doesn't seem any way to get the value from prefs from here. :(
  let expected = "image/webp,*/*";
  if (expected != request.getHeader("Accept")) {
    response.setStatusLine(request.httpVersion, 404, "Not Found");
    return;
  }

  response.setStatusLine(request.httpVersion, 302, "Moved Temporarily");
  response.setHeader("Location", "moz.png");
}
