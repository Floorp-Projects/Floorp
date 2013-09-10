/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test to see if the cached messages are displayed when the console UI is opened.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-webconsole-error-observer.html";

function test()
{
  waitForExplicitFinish();

  expectUncaughtException();

  addTab(TEST_URI);
  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
    testOpenUI(true);
  }, true);
}

function testOpenUI(aTestReopen)
{
  openConsole(null, function(hud) {
    waitForMessages({
      webconsole: hud,
      messages: [
        {
          text: "log Bazzle",
          category: CATEGORY_WEBDEV,
          severity: SEVERITY_LOG,
        },
        {
          text: "error Bazzle",
          category: CATEGORY_WEBDEV,
          severity: SEVERITY_ERROR,
        },
        {
          text: "bazBug611032",
          category: CATEGORY_JS,
          severity: SEVERITY_ERROR,
        },
        {
          text: "cssColorBug611032",
          category: CATEGORY_CSS,
          severity: SEVERITY_WARNING,
        },
      ],
    }).then(() => {
      closeConsole(gBrowser.selectedTab, function() {
        aTestReopen && info("will reopen the Web Console");
        executeSoon(aTestReopen ? testOpenUI : finishTest);
      });
    });
  });
}
