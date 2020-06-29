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

add_task(async function() {
  await promisePocketEnabled();

  let libraryButton = document.getElementById("library-button");
  libraryButton.click();

  let libraryView = document.getElementById("appMenu-libraryView");
  let popupShown = BrowserTestUtils.waitForEvent(libraryView, "ViewShown");
  await popupShown;

  checkElementsShown(true, ["appMenu-library-pocket-button"]);

  // Close the Library panel.
  let popupHidden = BrowserTestUtils.waitForEvent(document, "popuphidden");
  libraryView.closest("panel").hidePopup();

  // check context menu exists
  info("checking content context menu");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/browser/browser/components/pocket/test/test.html"
  );

  let contextMenu = document.getElementById("contentAreaContextMenu");
  popupShown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  popupHidden = BrowserTestUtils.waitForEvent(contextMenu, "popuphidden");
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "body",
    {
      type: "contextmenu",
      button: 2,
    },
    tab.linkedBrowser
  );
  await popupShown;

  checkElementsShown(true, ["pocket-button", "context-pocket"]);

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
  BrowserTestUtils.removeTab(tab);

  await promisePocketDisabled();

  checkElementsShown(false, [
    "context-pocket",
    "context-savelinktopocket",
    "appMenu-library-pocket-button",
    "pocket-button",
  ]);

  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  libraryButton = newWin.document.getElementById("library-button");
  libraryButton.click();

  libraryView = newWin.document.getElementById("appMenu-libraryView");
  popupShown = BrowserTestUtils.waitForEvent(libraryView, "ViewShown");
  await popupShown;

  checkElementsShown(
    false,
    [
      "context-pocket",
      "context-savelinktopocket",
      "appMenu-library-pocket-button",
      "pocket-button",
    ],
    newWin
  );

  // Close the Library panel.
  popupHidden = BrowserTestUtils.waitForEvent(newWin.document, "popuphidden");
  libraryView.closest("panel").hidePopup();

  await BrowserTestUtils.closeWindow(newWin);

  await promisePocketReset();
});
