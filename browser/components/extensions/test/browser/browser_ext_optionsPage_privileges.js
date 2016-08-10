/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* test_tab_options_privileges() {
  function backgroundScript() {
    browser.runtime.onMessage.addListener(({msgName, tabId}) => {
      if (msgName == "removeTabId") {
        browser.tabs.remove(tabId).then(() => {
          browser.test.notifyPass("options-ui-privileges");
        }).catch(error => {
          browser.test.log(`Error: ${error} :: ${error.stack}`);
          browser.test.notifyFail("options-ui-privileges");
        });
      }
    });
    browser.runtime.openOptionsPage();
  }

  function optionsScript() {
    browser.tabs.query({url: "http://example.com/"}).then(tabs => {
      browser.test.assertEq("http://example.com/", tabs[0].url, "Got the expect tab");
      return browser.tabs.getCurrent();
    }).then(tab => {
      browser.runtime.sendMessage({msgName: "removeTabId", tabId: tab.id});
    }).catch(error => {
      browser.test.log(`Error: ${error} :: ${error.stack}`);
      browser.test.notifyFail("options-ui-privileges");
    });
  }

  const ID = "options_privileges@tests.mozilla.org";
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",

    manifest: {
      applications: {gecko: {id: ID}},
      "permissions": ["tabs"],
      "options_ui": {
        "page": "options.html",
      },
    },
    files: {
      "options.html": `<!DOCTYPE html>
        <html>
          <head>
            <meta charset="utf-8">
            <script src="options.js" type="text/javascript"></script>
          </head>
        </html>`,
      "options.js": optionsScript,
    },
    background: backgroundScript,
  });

  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");

  yield extension.startup();

  yield extension.awaitFinish("options-ui-privileges");

  yield extension.unload();

  yield BrowserTestUtils.removeTab(tab);
});
