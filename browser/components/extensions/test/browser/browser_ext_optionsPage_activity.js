/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_options_activity() {
  async function backgroundScript() {
    browser.runtime.openOptionsPage();
  }

  function optionsScript() {
    browser.test.sendMessage("options-page:loaded", document.documentURI);
  }

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",

    manifest: {
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
          <body style="height: 100px;">
            <h1>Extensions Options</h1>
            <a href="https://example.com/options-page-link">options page link</a>
          </body>
        </html>`,
      "options.js": optionsScript,
    },
    background: backgroundScript,
  });

  const aboutAddonsTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:addons"
  );

  await extension.startup();

  await extension.awaitMessage("options-page:loaded");

  const optionsBrowser = getInlineOptionsBrowser(gBrowser.selectedBrowser);

  ok(
    !optionsBrowser.ownerDocument.hidden,
    "Parent should be active since it's in the foreground"
  );
  ok(
    optionsBrowser.docShellIsActive,
    "Should be active since we're in the foreground"
  );

  let parentVisibilityChange = BrowserTestUtils.waitForEvent(
    optionsBrowser.ownerDocument,
    "visibilitychange"
  );
  const aboutBlankTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  await parentVisibilityChange;
  ok(
    !optionsBrowser.docShellIsActive,
    "Should become inactive since parent was backgrounded"
  );

  BrowserTestUtils.removeTab(aboutBlankTab);
  BrowserTestUtils.removeTab(aboutAddonsTab);

  await extension.unload();
});
