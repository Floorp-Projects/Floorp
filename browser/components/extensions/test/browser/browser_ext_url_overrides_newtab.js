/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

const NEWTAB_URI_1 = "webext-newtab-1.html";

add_task(async function test_sending_message_from_newtab_page() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "chrome_url_overrides": {
        newtab: NEWTAB_URI_1,
      },
    },
    useAddonManager: "temporary",
    files: {
      [NEWTAB_URI_1]: `
        <!DOCTYPE html>
        <head>
          <meta charset="utf-8"/></head>
        <html>
          <body>
            <script src="newtab.js"></script>
          </body>
        </html>
      `,

      "newtab.js": function() {
        window.onload = () => {
          browser.test.sendMessage("from-newtab-page", window.location.href);
        };
      },
    },
  });

  await extension.startup();

  // Simulate opening the newtab open as a user would.
  BrowserOpenTab();

  let url = await extension.awaitMessage("from-newtab-page");
  ok(url.endsWith(NEWTAB_URI_1),
     "Newtab url is overriden by the extension.");

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await extension.unload();
});
