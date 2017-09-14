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

/**
 * Ensure we don't show the extension URL in the URL bar temporarily in new tabs
 * while we're switching remoteness (when the URL we're loading and the
 * default content principal are different).
 */
add_task(async function dontTemporarilyShowAboutExtensionPath() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test Extension",
      applications: {
        gecko: {
          id: "newtab@mochi.test",
        },
      },
      chrome_url_overrides: {
        newtab: "newtab.html",
      },
    },
    background() {
      browser.test.sendMessage("url", browser.runtime.getURL("newtab.html"));
    },
    files: {
      "newtab.html": "<h1>New tab!</h1>",
    },
    useAddonManager: "temporary",
  });

  await extension.startup();
  let url = await extension.awaitMessage("url");

  let wpl = {
    onLocationChange() {
      is(gURLBar.value, "", "URL bar value should stay empty.");
    },
  };
  gBrowser.addProgressListener(wpl);

  let tab = await BrowserTestUtils.openNewForegroundTab({gBrowser, url});

  gBrowser.removeProgressListener(wpl);
  is(gURLBar.value, "", "URL bar value should be empty.");
  ContentTask.spawn(tab.linkedBrowser, null, function() {
    is(content.document.body.textContent, "New tab!", "New tab page is loaded.");
  });

  await BrowserTestUtils.removeTab(tab);
  await extension.unload();
});
