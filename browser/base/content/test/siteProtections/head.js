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
  let shieldIconContainer = document.getElementById(
    "tracking-protection-icon-container"
  );

  // Move out than move over the shield icon to trigger the hover event in
  // order to fetch tracker count.
  EventUtils.synthesizeMouseAtCenter(gURLBar.textbox, {
    type: "mousemove",
  });
  EventUtils.synthesizeMouseAtCenter(shieldIconContainer, {
    type: "mousemove",
  });

  if (!toast) {
    EventUtils.synthesizeMouseAtCenter(shieldIconContainer, {});
  } else {
    gProtectionsHandler.showProtectionsPopup({ toast });
  }

  await popupShownPromise;
}

async function openProtectionsPanelWithKeyNav() {
  let popupShownPromise = BrowserTestUtils.waitForEvent(
    protectionsPopup,
    "popupshown"
  );

  gURLBar.focus();

  // This will trigger the focus event for the shield icon for pre-fetching
  // the tracker count.
  EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
  EventUtils.synthesizeKey("KEY_Enter", {});

  await popupShownPromise;
}

async function closeProtectionsPanel() {
  let popuphiddenPromise = BrowserTestUtils.waitForEvent(
    protectionsPopup,
    "popuphidden"
  );

  PanelMultiView.hidePopup(protectionsPopup);
  await popuphiddenPromise;
}
