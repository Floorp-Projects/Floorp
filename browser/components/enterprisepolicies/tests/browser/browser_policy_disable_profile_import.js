/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

async function openLibrary() {
  return new Promise(resolve => {
    let library = window.openDialog("chrome://browser/content/places/places.xul", "",
                                    "chrome,toolbar=yes,dialog=no,resizable");
    waitForFocus(() => resolve(library), library);
  });
}

add_task(async function test_disable_profile_import() {
  await setupPolicyEngineWithJson({
    "policies": {
      "DisableProfileImport": true,
    },
  });
  let library = await openLibrary();

  let menu = library.document.getElementById("maintenanceButtonPopup");
  let promisePopupShown = BrowserTestUtils.waitForEvent(menu, "popupshown");
  menu.openPopup();
  await promisePopupShown;

  let profileImportButton = library.document.getElementById("browserImport");
  is(profileImportButton.disabled, true, "Profile Import button should be disabled");

  let promisePopupHidden = BrowserTestUtils.waitForEvent(menu, "popuphidden");
  menu.hidePopup();
  await promisePopupHidden;

  await BrowserTestUtils.closeWindow(library);

  checkLockedPref("browser.newtabpage.activity-stream.migrationExpired", true);
});
