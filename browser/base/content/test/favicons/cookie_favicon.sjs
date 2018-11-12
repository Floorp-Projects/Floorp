/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function handleRequest(request, response) {
  let state = getState("cache_cookie");
  if (!state) {
    state = 0;
  }

  response.setStatusLine(request.httpVersion, 302, "Moved Temporarily");
  response.setHeader("Set-Cookie", `faviconCookie=${++state}`);
  response.setHeader("Location", "http://example.com/browser/browser/base/content/test/favicons/moz.png");
  setState("cache_cookie", `${state}`);
}
