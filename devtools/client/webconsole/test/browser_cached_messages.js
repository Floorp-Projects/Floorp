/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test to see if the cached messages are displayed when the console UI is
// opened.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
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
