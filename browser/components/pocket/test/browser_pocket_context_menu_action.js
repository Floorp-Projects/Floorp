/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

add_task(async function () {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/browser/browser/components/pocket/test/test.html"
  );

  info("opening context menu");
  let contextMenu = document.getElementById("contentAreaContextMenu");
  let popupShown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  let popupHidden = BrowserTestUtils.waitForEvent(contextMenu, "popuphidden");

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "body",
    {
      type: "contextmenu",
      button: 2,
    },
    tab.linkedBrowser
  );
  await popupShown;

  info("opening pocket panel");
  let contextPocket = contextMenu.querySelector("#context-pocket");
  // The panel is created on the fly, so we can't simply wait for focus
  // inside it.
  let pocketPanelShown = BrowserTestUtils.waitForEvent(
    document,
    "popupshown",
    true
  );
  contextMenu.activateItem(contextPocket);
  await pocketPanelShown;
  checkElements(true, ["customizationui-widget-panel"]);

  info("closing pocket panel");
  let pocketPanel = document.getElementById("customizationui-widget-panel");
  let pocketPanelHidden = BrowserTestUtils.waitForEvent(
    pocketPanel,
    "popuphidden"
  );

  pocketPanel.hidePopup();
  await pocketPanelHidden;
  checkElements(false, ["customizationui-widget-panel"]);

  contextMenu.hidePopup();
  await popupHidden;
  BrowserTestUtils.removeTab(tab);
});
