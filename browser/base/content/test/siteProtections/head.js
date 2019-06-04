/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var protectionsPopup = document.getElementById("protections-popup");

async function openProtectionsPanel() {
  let popupShownPromise = BrowserTestUtils.waitForEvent(protectionsPopup, "popupshown");
  let identityBox = document.getElementById("identity-box");
  EventUtils.synthesizeMouseAtCenter(identityBox, { metaKey: true });
  await popupShownPromise;
}
