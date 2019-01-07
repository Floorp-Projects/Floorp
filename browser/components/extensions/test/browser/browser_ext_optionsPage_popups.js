/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_tab_options_popups() {
  function backgroundScript() {
    browser.runtime.openOptionsPage();
  }

  function optionsScript() {
    browser.test.sendMessage("options-page:loaded");
  }

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",

    manifest: {
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
          <body style="height: 100px;">
            <h1>Extensions Options</h1>
            <a href="http://mochi.test:8888/">options page link</a>
          </body>
        </html>`,
      "options.js": optionsScript,
    },
    background: backgroundScript,
  });

  const aboutAddonsTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:addons");

  await extension.startup();

  // Wait the options page to be loaded.
  await extension.awaitMessage("options-page:loaded");

  const optionsBrowser = gBrowser.selectedBrowser.contentDocument.getElementById("addon-options");
  let contentAreaContextMenu = document.getElementById("contentAreaContextMenu");
  let popupShownPromise = BrowserTestUtils.waitForEvent(contentAreaContextMenu, "popupshown");

  await BrowserTestUtils.waitForCondition(async () => {
    BrowserTestUtils.synthesizeMouseAtCenter("a", {type: "contextmenu"}, optionsBrowser);

    // It looks that syntesizeMouseAtCenter is sometimes able to trigger the mouse event on the
    // HTML document instead of triggering it on the expected document element while running this
    // test in --verify mode, and we are going to send the mouse event again if it didn't
    // triggered the context menu yet.
    return Promise.race([
      popupShownPromise.then(() => true),
      delay(500).then(() => false),
    ]);
  }, "Waiting the context menu to be shown");

  let contextMenuItemIds = [
    "context-openlinkintab",
    "context-openlinkprivate",
    "context-copylink",
    "context-openlinkinusercontext-menu",
  ];

  for (const itemID of contextMenuItemIds) {
    const item = contentAreaContextMenu.querySelector(`#${itemID}`);

    ok(!item.hidden, `${itemID} should not be hidden`);
    ok(!item.disabled, `${itemID} should not be disabled`);
  }

  // Close the context menu and ensure that it is gone, otherwise there
  // may be intermittent failures related to shutdown leaks.
  await BrowserTestUtils.waitForCondition(async () => {
    let popupHiddenPromise = BrowserTestUtils.waitForEvent(contentAreaContextMenu, "popuphidden");
    contentAreaContextMenu.hidePopup();
    await popupHiddenPromise;
    return contentAreaContextMenu.state === "closed";
  }, "Wait context menu popup to be closed");

  BrowserTestUtils.removeTab(aboutAddonsTab);

  await extension.unload();
});
