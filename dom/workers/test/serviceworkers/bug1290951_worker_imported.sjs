/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

function handleRequest(request, response) {
  let count = getState("count");
  if (count === "") {
    count = "1";
  }

  // If this is the first request, return the first source.
  if (count === "1") {
    response.setHeader("Content-Type", "application/javascript");
    response.write("// Imported.");
    setState("count", "2");
  }
  // For all subsequent requests, return the second source.
  else {
    response.setStatusLine(request.httpVersion, 404, "Not found");
  }
}

