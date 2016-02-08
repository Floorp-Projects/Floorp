/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the page's resources are displayed in the console as they're
// loaded

"use strict";

const TEST_NETWORK_URI = "http://example.com/browser/devtools/client/" +
                         "webconsole/test/test-network.html" + "?_date=" +
                         Date.now();

add_task(function* () {
  yield loadTab("data:text/html;charset=utf-8,Web Console basic network " +
                "logging test");
  let hud = yield openConsole();

  yield BrowserTestUtils.loadURI(gBrowser.selectedBrowser, TEST_NETWORK_URI);

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
