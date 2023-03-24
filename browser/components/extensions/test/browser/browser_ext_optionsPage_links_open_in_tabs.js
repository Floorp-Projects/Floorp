/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_options_links() {
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

  const promiseNewTabOpened = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "https://example.com/options-page-link"
  );
  await SpecialPowers.spawn(optionsBrowser, [], () =>
    content.document.querySelector("a").click()
  );
  info(
    "Expect a new tab to be opened when a link is clicked in the options_page embedded inside about:addons"
  );
  const newTab = await promiseNewTabOpened;
  ok(newTab, "Got a new tab created on the expected url");
  BrowserTestUtils.removeTab(newTab);

  BrowserTestUtils.removeTab(aboutAddonsTab);

  await extension.unload();
});
