/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function handleRequest(request, response) {
  let page = "download";
  response.setStatusLine(request.httpVersion, "200", "OK");

  let [first, second] = request.queryString.split("=");
  let headerStr = first;
  if (second !== "none") {
    headerStr += "; filename=" + second;
  }

  response.setHeader("Content-Disposition", headerStr);
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Content-Length", page.length + "", false);
  response.write(page);
}
