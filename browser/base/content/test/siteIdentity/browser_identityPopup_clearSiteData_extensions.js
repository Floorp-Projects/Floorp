/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/*
 *  Test for Bug 1661534 - Extension page: "Clear Cookies and Site Data"
 *  does nothing.
 *
 *  Expected behavior: when viewing a page controlled by a WebExtension,
 *  the "Clear Cookies and Site Data..." button should not be visible.
 */

add_task(async function testClearSiteDataFooterHiddenForExtensions() {
  // Create an extension that opens an options page
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",

    manifest: {
      permissions: ["tabs"],
      options_ui: {
        page: "options.html",
        open_in_tab: true,
      },
    },
    files: {
      "options.html": `<!DOCTYPE html>
        <html>
          <head>
            <meta charset="utf-8">
          </head>
          <body>
            <h1>This is a test options page for a WebExtension</h1>
          </body>
        </html>`,
    },
    async background() {
      await browser.runtime.openOptionsPage();
      browser.test.sendMessage("optionsopened");
    },
  });

  // Run the extension and wait until its options page has finished loading
  let browser = gBrowser.selectedBrowser;
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(browser);
  await extension.startup();
  await extension.awaitMessage("optionsopened");
  await browserLoadedPromise;

  await SpecialPowers.spawn(browser, [], () => {
    ok(
      content.document.documentURI.startsWith("moz-extension://"),
      "Extension page has now finished loading in the browser window"
    );
  });

  // Open the site identity popup
  let { gIdentityHandler } = gBrowser.ownerGlobal;
  let promisePanelOpen = BrowserTestUtils.waitForEvent(
    gBrowser.ownerGlobal,
    "popupshown",
    true,
    event => event.target == gIdentityHandler._identityPopup
  );
  gIdentityHandler._identityIconBox.click();
  await promisePanelOpen;

  let clearSiteDataFooter = document.getElementById(
    "identity-popup-clear-sitedata-footer"
  );

  ok(
    clearSiteDataFooter.hidden,
    "The clear site data footer is hidden on a WebExtension page."
  );

  // Unload the extension
  await extension.unload();
});
