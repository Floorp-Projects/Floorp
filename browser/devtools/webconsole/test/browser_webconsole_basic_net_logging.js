/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that the page's resources are displayed in the console as they're
// loaded

const TEST_NETWORK_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-network.html" + "?_date=" + Date.now();

function test() {
  addTab("data:text/html;charset=utf-8,Web Console basic network logging test");
  browser.addEventListener("load", onLoad, true);

  function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, function(hud) {
      content.location = TEST_NETWORK_URI;
      waitForMessages({
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
      }).then(finishTest);
    });
  }
}
