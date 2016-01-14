/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that network log messages bring up the network panel.

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console network " +
                 "logging tests";

const TEST_NETWORK_REQUEST_URI =
  "http://example.com/browser/devtools/client/webconsole/test/" +
  "test-network-request.html";

var hud;

function test() {
  loadTab(TEST_URI).then((tab) => {
    HUDService.openBrowserConsoleOrFocus().then(aHud => {
      hud = aHud;
      HUDService.lastFinishedRequest.callback = testResponse;
      BrowserTestUtils.loadURI(gBrowser.selectedBrowser,
                               TEST_NETWORK_REQUEST_URI);
    });
  });
}

function testResponse(request) {
  hud.ui.webConsoleClient.getResponseContent(request.actor,
    function(contentPacket) {
      hud.ui.webConsoleClient.getRequestPostData(request.actor,
        function(postDataPacket) {
          // Check if page load was logged correctly.
          ok(request, "Page load was logged");

          is(request.request.url, TEST_NETWORK_REQUEST_URI,
            "Logged network entry is page load");
          is(request.request.method, "GET", "Method is correct");
          ok(!postDataPacket.postData.text, "No request body was stored");
          ok(postDataPacket.postDataDiscarded, "Request body was discarded");
          ok(!contentPacket.content.text, "No response body was stored");
          ok(contentPacket.contentDiscarded || request.fromCache,
             "Response body was discarded or response came from the cache");

          executeSoon(finishTest);
        });
    });
}
