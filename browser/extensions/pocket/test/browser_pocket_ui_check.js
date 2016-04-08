"use strict";

function checkWindowProperties(expectPresent, l) {
  for (let name of l) {
    is(!!window.hasOwnProperty(name), expectPresent, "property " + name + (expectPresent ? " is" : " is not") + " present");
  }
}
function checkElements(expectPresent, l) {
  for (let id of l) {
    is(!!document.getElementById(id), expectPresent, "element " + id + (expectPresent ? " is" : " is not") + " present");
  }
}

add_task(function* test_setup() {
  let clearValue = Services.prefs.prefHasUserValue("extensions.pocket.enabled");
  let enabledOnStartup = Services.prefs.getBoolPref("extensions.pocket.enabled");
  registerCleanupFunction(() => {
    if (clearValue) {
      Services.prefs.clearUserPref("extensions.pocket.enabled");
    } else {
      Services.prefs.setBoolPref("extensions.pocket.enabled", enabledOnStartup);
    }
  });
});

add_task(function*() {
  yield promisePocketEnabled();

  checkWindowProperties(true, ["Pocket", "pktUI", "pktUIMessaging"]);
  checkElements(true, ["pocket-button", "panelMenu_pocket", "menu_pocket", "BMB_pocket",
                       "panelMenu_pocketSeparator", "menu_pocketSeparator",
                       "BMB_pocketSeparator"]);

  // check context menu exists
  info("checking content context menu");
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "https://example.com/browser/browser/extensions/pocket/test/test.html");

  let contextMenu = document.getElementById("contentAreaContextMenu");
  let popupShown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  let popupHidden = BrowserTestUtils.waitForEvent(contextMenu, "popuphidden");
  yield BrowserTestUtils.synthesizeMouseAtCenter("body", {
    type: "contextmenu",
    button: 2
  }, tab.linkedBrowser);
  yield popupShown;

  checkElements(true, ["context-pocket", "context-savelinktopocket"]);

  contextMenu.hidePopup();
  yield popupHidden;
  yield BrowserTestUtils.removeTab(tab);

  yield promisePocketDisabled();

  checkWindowProperties(false, ["Pocket", "pktUI", "pktUIMessaging"]);
  checkElements(false, ["pocket-button", "panelMenu_pocket", "menu_pocket", "BMB_pocket",
                       "panelMenu_pocketSeparator", "menu_pocketSeparator",
                       "BMB_pocketSeparator", "context-pocket", "context-savelinktopocket"]);

  yield promisePocketReset();
});
