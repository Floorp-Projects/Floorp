/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that the page's resources are displayed in the console as they're
// loaded

"use strict";

const TEST_NETWORK_URI = "http://example.com/browser/browser/devtools/" +
                         "webconsole/test/test-network.html" + "?_date=" +
                         Date.now();

var test = asyncTest(function* () {
  yield loadTab("data:text/html;charset=utf-8,Web Console basic network " +
                "logging test");
  let hud = yield openConsole();

  content.location = TEST_NETWORK_URI;

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "running network console",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    },
    {
      text: "test-network.html",
      category: CATEGORY_NETWORK,
      severity: SEVERITY_LOG,
    },
    {
      text: "testscript.js",
      category: CATEGORY_NETWORK,
      severity: SEVERITY_LOG,
    },
    {
      text: "test-image.png",
      category: CATEGORY_NETWORK,
      severity: SEVERITY_LOG,
    }],
  });
});
