/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This provides the tests with a page with different titles based on whether
// a cookie is present or not.

function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);

  var cookie = "name=value";
  var title = "No Cookie";
  if (request.hasHeader("Cookie") && request.getHeader("Cookie") == cookie)
    title = "Cookie";
  else
    response.setHeader("Set-Cookie", cookie, false);

  response.write("<html><head><title>");
  response.write(title);
  response.write("</title><body>test page</body></html>");
}
