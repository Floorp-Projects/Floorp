/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that console logging methods display the method location along with
// the output in the console.

const TEST_URI = "http://example.com/browser/browser/devtools/" +
                 "webconsole/test/" +
                 "test-bug-646025-console-file-location.html";

function test() {
  addTab("data:text/html;charset=utf-8,Web Console file location display test");
  browser.addEventListener("load", onLoad, true);
}

function onLoad(aEvent) {
  browser.removeEventListener(aEvent.type, onLoad, true);
  openConsole(null, function(hud) {
    content.location = TEST_URI;
    waitForMessages({
      webconsole: hud,
      messages: [{
        text: "message for level log",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
        source: { url: "test-file-location.js", line: 5 },
      },
      {
        text: "message for level info",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_INFO,
        source: { url: "test-file-location.js", line: 6 },
      },
      {
        text: "message for level warn",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_WARNING,
        source: { url: "test-file-location.js", line: 7 },
      },
      {
        text: "message for level error",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_ERROR,
        source: { url: "test-file-location.js", line: 8 },
      },
      {
        text: "message for level debug",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
        source: { url: "test-file-location.js", line: 9 },
      }],
    }).then(finishTest);
  });
}
