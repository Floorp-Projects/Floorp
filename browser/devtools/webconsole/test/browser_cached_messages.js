/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test to see if the cached messages are displayed when the console UI is
// opened.

"use strict";

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/" +
                 "test/test-webconsole-error-observer.html";

// On e10s, the exception is triggered in child process
// and is ignored by test harness
if (!Services.appinfo.browserTabsRemoteAutostart) {
  expectUncaughtException();
}

function test() {
  waitForExplicitFinish();

  loadTab(TEST_URI).then(testOpenUI);
}

function testOpenUI(aTestReopen) {
  openConsole().then((hud) => {
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
      closeConsole(gBrowser.selectedTab).then(() => {
        aTestReopen && info("will reopen the Web Console");
        executeSoon(aTestReopen ? testOpenUI : finishTest);
      });
    });
  });
}
