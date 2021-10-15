/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function handleRequest(request, response)
{
  let page = "<!DOCTYPE html><html><body><p>HSTS page</p></body></html>";
  response.setStatusLine(request.httpVersion, "200", "OK");
  response.setHeader("Strict-Transport-Security", "max-age=60");
  response.setHeader("Content-Type", "text/html", false);
  response.setHeader("Content-Length", page.length + "", false);
  response.write(page);
}
