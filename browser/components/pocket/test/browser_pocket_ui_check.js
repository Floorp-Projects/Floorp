"use strict";

add_task(async function test_setup() {
  let clearValue = Services.prefs.prefHasUserValue("extensions.pocket.enabled");
  let enabledOnStartup = Services.prefs.getBoolPref(
    "extensions.pocket.enabled"
  );
  registerCleanupFunction(() => {
    if (clearValue) {
      Services.prefs.clearUserPref("extensions.pocket.enabled");
    } else {
      Services.prefs.setBoolPref("extensions.pocket.enabled", enabledOnStartup);
    }
  });
});

add_task(async function () {
  await promisePocketEnabled();

  // check context menu exists
  info("checking content context menu");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/browser/browser/components/pocket/test/test.html"
  );

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

  checkElementsShown(true, ["save-to-pocket-button", "context-pocket"]);

  contextMenu.hidePopup();
  await popupHidden;
  popupShown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  popupHidden = BrowserTestUtils.waitForEvent(contextMenu, "popuphidden");
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "a",
    {
      type: "contextmenu",
      button: 2,
    },
    tab.linkedBrowser
  );
  await popupShown;

  checkElementsShown(true, ["context-savelinktopocket"]);
  contextMenu.hidePopup();
  await popupHidden;

  await promisePocketDisabled();

  popupShown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  popupHidden = BrowserTestUtils.waitForEvent(contextMenu, "popuphidden");
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "a",
    {
      type: "contextmenu",
      button: 2,
    },
    tab.linkedBrowser
  );
  await popupShown;

  checkElementsShown(false, [
    "context-pocket",
    "context-savelinktopocket",
    "save-to-pocket-button",
  ]);

  contextMenu.hidePopup();
  await popupHidden;
  BrowserTestUtils.removeTab(tab);

  await promisePocketReset();
});
