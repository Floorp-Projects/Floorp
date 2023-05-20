/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_popup_links_open_tabs() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {
        default_popup: "popup.html",
      },
    },

    files: {
      "popup.html": `<!DOCTYPE html>
        <html>
          <head>
            <meta charset="utf-8">
            <script src="popup.js" type="text/javascript"></script>
          </head>
          <body style="height: 100px;">
            <h1>Extension Popup</h1>
            <a href="https://example.com/popup-page-link">popup page link</a>
          </body>
        </html>`,
      "popup.js": function () {
        window.onload = () => {
          browser.test.sendMessage("from-popup", "popup-a");
        };
      },
    },
  });

  await extension.startup();

  let widget = getBrowserActionWidget(extension);
  CustomizableUI.addWidgetToArea(widget.id, CustomizableUI.AREA_NAVBAR, 0);

  let promiseActionPopupBrowser = awaitExtensionPanel(extension);
  clickBrowserAction(extension);
  await extension.awaitMessage("from-popup");
  let popupBrowser = await promiseActionPopupBrowser;
  const promiseNewTabOpened = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "https://example.com/popup-page-link"
  );
  await SpecialPowers.spawn(popupBrowser, [], () =>
    content.document.querySelector("a").click()
  );
  const newTab = await promiseNewTabOpened;
  ok(newTab, "Got a new tab created on the expected url");
  BrowserTestUtils.removeTab(newTab);

  await closeBrowserAction(extension);
  await extension.unload();
});
