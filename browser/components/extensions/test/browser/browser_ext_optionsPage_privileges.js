/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_tab_options_privileges() {
  function backgroundScript() {
    browser.runtime.onMessage.addListener(async ({ msgName, tab }) => {
      if (msgName == "removeTab") {
        try {
          const [activeTab] = await browser.tabs.query({ active: true });
          browser.test.assertEq(
            tab.id,
            activeTab.id,
            "tabs.getCurrent has got the expected tabId"
          );
          browser.test.assertEq(
            tab.windowId,
            activeTab.windowId,
            "tabs.getCurrent has got the expected windowId"
          );
          await browser.tabs.remove(tab.id);

          browser.test.notifyPass("options-ui-privileges");
        } catch (error) {
          browser.test.log(`Error: ${error} :: ${error.stack}`);
          browser.test.notifyFail("options-ui-privileges");
        }
      }
    });
    browser.runtime.openOptionsPage();
  }

  async function optionsScript() {
    try {
      let [tab] = await browser.tabs.query({ url: "http://example.com/" });
      browser.test.assertEq(
        "http://example.com/",
        tab.url,
        "Got the expect tab"
      );

      tab = await browser.tabs.getCurrent();
      browser.runtime.sendMessage({ msgName: "removeTab", tab });
    } catch (error) {
      browser.test.log(`Error: ${error} :: ${error.stack}`);
      browser.test.notifyFail("options-ui-privileges");
    }
  }

  const ID = "options_privileges@tests.mozilla.org";
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",

    manifest: {
      applications: { gecko: { id: ID } },
      permissions: ["tabs"],
      options_ui: {
        page: "options.html",
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

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com/"
  );

  await extension.startup();

  await extension.awaitFinish("options-ui-privileges");

  await extension.unload();

  BrowserTestUtils.removeTab(tab);
});
