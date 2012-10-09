/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This provides the tests with a download URL which never finishes.

var timer;

function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.processAsync();

  const nsITimer = Components.interfaces.nsITimer;

  function stall() {
    timer = null;
    // This write will throw if the connection has been closed by the browser.
    response.write("stalling...\n");
    timer = Components.classes["@mozilla.org/timer;1"]
                        .createInstance(nsITimer);
    timer.initWithCallback(stall, 500, nsITimer.TYPE_ONE_SHOT);
  }

  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Accept-Ranges", "none", false);
  stall();
}
