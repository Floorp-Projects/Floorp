/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* testExecuteScriptAtOnUpdated() {
  const BASE = "http://mochi.test:8888/browser/browser/components/extensions/test/browser/";
  const URL = BASE + "file_iframe_document.html";
  // This is a regression test for bug 1325830.
  // The bug (executeScript not completing any more) occurred when executeScript
  // was called early at the onUpdated event, unless the tabs.create method is
  // called. So this test does not use tabs.create to open new tabs.
  // Note that if this test is run together with other tests that do call
  // tabs.create, then this test case does not properly test the conditions of
  // the regression any more. To verify that the regression has been resolved,
  // this test must be run in isolation.

  function background() {
    // Using variables to prevent listeners from running more than once, instead
    // of removing the listener. This is to minimize any IPC, since the bug that
    // is being tested is sensitive to timing.
    let ignore = false;
    let url;
    browser.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {
      if (changeInfo.status === "loading" && tab.url === url && !ignore) {
        ignore = true;
        browser.tabs.executeScript(tabId, {
          code: "document.URL",
        }).then(results => {
          browser.test.assertEq(url, results[0], "Content script should run");
          browser.test.notifyPass("executeScript-at-onUpdated");
        }, error => {
          browser.test.fail(`Unexpected error: ${error} :: ${error.stack}`);
          browser.test.notifyFail("executeScript-at-onUpdated");
        });
        // (running this log call after executeScript to minimize IPC between
        //  onUpdated and executeScript.)
        browser.test.log(`Found expected navigation to ${url}`);
      } else {
        // The bug occurs when executeScript is called before a tab is
        // initialized.
        browser.tabs.executeScript(tabId, {code: ""});
      }
    });
    browser.test.onMessage.addListener(testUrl => {
      url = testUrl;
      browser.test.sendMessage("open-test-tab");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["http://mochi.test/", "tabs"],
    },
    background,
  });

  yield extension.startup();
  extension.sendMessage(URL);
  yield extension.awaitMessage("open-test-tab");
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, URL, true);

  yield extension.awaitFinish("executeScript-at-onUpdated");

  yield extension.unload();

  yield BrowserTestUtils.removeTab(tab);
});
