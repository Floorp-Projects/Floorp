/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

/**
 * Serve a different content, each time it is loaded
 */
const contents = `console.log("Same url script #COUNTER");`;

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-store");
  response.setHeader("Content-Type", "application/javascript");

  let counter = 1 + parseInt(getState("counter") || 0);
  setState("counter", "" + counter);

  response.write(contents.replace(/COUNTER/g, counter));
}
