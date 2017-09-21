"use strict";

function checkWindowProperties(expectPresent, l) {
  for (let name of l) {
    is(!!window.hasOwnProperty(name), expectPresent, "property " + name + (expectPresent ? " is" : " is not") + " present");
  }
}
function checkElements(expectPresent, l) {
  for (let id of l) {
    let el = document.getElementById(id) || gNavToolbox.palette.querySelector("#" + id);
    is(!!el, expectPresent, "element " + id + (expectPresent ? " is" : " is not") + " present");
  }
}

add_task(async function test_setup() {
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

add_task(async function() {
  await promisePocketEnabled();

  checkWindowProperties(true, ["Pocket", "pktUI", "pktUIMessaging"]);
  checkElements(true, ["pocket-button", "panelMenu_pocket",
                       "panelMenu_pocketSeparator"]);

  // check context menu exists
  info("checking content context menu");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "https://example.com/browser/browser/extensions/pocket/test/test.html");

  let contextMenu = document.getElementById("contentAreaContextMenu");
  let popupShown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  let popupHidden = BrowserTestUtils.waitForEvent(contextMenu, "popuphidden");
  await BrowserTestUtils.synthesizeMouseAtCenter("body", {
    type: "contextmenu",
    button: 2
  }, tab.linkedBrowser);
  await popupShown;

  checkElements(true, ["context-pocket", "context-savelinktopocket"]);

  contextMenu.hidePopup();
  await popupHidden;
  await BrowserTestUtils.removeTab(tab);

  await promisePocketDisabled();

  checkWindowProperties(false, ["Pocket", "pktUI", "pktUIMessaging"]);
  checkElements(false, ["pocket-button", "panelMenu_pocket", "panelMenu_pocketSeparator",
                        "context-pocket", "context-savelinktopocket"]);

  await promisePocketReset();
});
