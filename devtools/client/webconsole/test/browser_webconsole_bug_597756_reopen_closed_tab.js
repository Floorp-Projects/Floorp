/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-bug-597756-reopen-closed-tab.html";

var HUD;

var test = asyncTest(function* () {
  // On e10s, the exception is triggered in child process
  // and is ignored by test harness
  if (!Services.appinfo.browserTabsRemoteAutostart) {
    expectUncaughtException();
  }

  let { browser } = yield loadTab(TEST_URI);
  HUD = yield openConsole();

  if (!Services.appinfo.browserTabsRemoteAutostart) {
    expectUncaughtException();
  }

  yield reload(browser);

  yield testMessages();

  yield closeConsole();

  // Close and reopen
  gBrowser.removeCurrentTab();

  if (!Services.appinfo.browserTabsRemoteAutostart) {
    expectUncaughtException();
  }

  let tab = yield loadTab(TEST_URI);
  HUD = yield openConsole();

  if (!Services.appinfo.browserTabsRemoteAutostart) {
    expectUncaughtException();
  }

  yield reload(tab.browser);

  yield testMessages();

  HUD = null;
});

function reload(browser) {
  let loaded = loadBrowser(browser);
  content.location.reload();
  return loaded;
}

function testMessages() {
  return waitForMessages({
    webconsole: HUD,
    messages: [{
      name: "error message displayed",
      text: "fooBug597756_error",
      category: CATEGORY_JS,
      severity: SEVERITY_ERROR,
    }],
  });
}
