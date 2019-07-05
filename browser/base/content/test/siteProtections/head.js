/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var protectionsPopup = document.getElementById("protections-popup");
var protectionsPopupMainView = document.getElementById(
  "protections-popup-mainView"
);
var protectionsPopupHeader = document.getElementById(
  "protections-popup-mainView-panel-header"
);

async function openProtectionsPanel(toast) {
  let popupShownPromise = BrowserTestUtils.waitForEvent(
    protectionsPopup,
    "popupshown"
  );
  let identityBox = document.getElementById("identity-box");
  if (!toast) {
    EventUtils.synthesizeMouseAtCenter(identityBox, { altKey: true });
  } else {
    gProtectionsHandler.showProtectionsPopup({ toast });
  }

  await popupShownPromise;
}
