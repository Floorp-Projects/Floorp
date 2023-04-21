/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

async function openLibrary() {
  return new Promise(resolve => {
    let library = window.openDialog(
      "chrome://browser/content/places/places.xhtml",
      "",
      "chrome,toolbar=yes,dialog=no,resizable"
    );
    waitForFocus(() => resolve(library), library);
  });
}

add_task(async function test_disable_profile_import() {
  await setupPolicyEngineWithJson({
    policies: {
      DisableProfileImport: true,
    },
  });
  let library = await openLibrary();

  let menu = library.document.getElementById("maintenanceButtonPopup");
  let promisePopupShown = BrowserTestUtils.waitForEvent(menu, "popupshown");
  menu.openPopup();
  await promisePopupShown;

  let profileImportButton = library.document.getElementById("browserImport");
  is(
    profileImportButton.disabled,
    true,
    "Profile Import button should be disabled"
  );

  let promisePopupHidden = BrowserTestUtils.waitForEvent(menu, "popuphidden");
  menu.hidePopup();
  await promisePopupHidden;

  await BrowserTestUtils.closeWindow(library);

  checkLockedPref("browser.newtabpage.activity-stream.migrationExpired", true);
});

add_task(async function test_file_menu() {
  gFileMenu.updateImportCommandEnabledState();

  let command = document.getElementById("cmd_file_importFromAnotherBrowser");
  ok(
    command.getAttribute("disabled"),
    "The `Import from Another Browser…` File menu item command should be disabled"
  );

  if (Services.appinfo.OS == "Darwin") {
    // We would need to have a lot of boilerplate to open the menus on Windows
    // and Linux to test this there.
    let menuitem = document.getElementById("menu_importFromAnotherBrowser");
    ok(
      menuitem.disabled,
      "The `Import from Another Browser…` File menu item should be disabled"
    );
  }
});

add_task(async function test_import_button() {
  await PlacesUIUtils.maybeAddImportButton();
  ok(
    !document.getElementById("import-button"),
    "Import button should be hidden."
  );
});

add_task(async function test_prefs_entrypoint() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.migrate.preferences-entrypoint.enabled", true]],
  });

  let finalPaneEvent = Services.prefs.getBoolPref("identity.fxaccounts.enabled")
    ? "sync-pane-loaded"
    : "privacy-pane-loaded";
  let finalPrefPaneLoaded = TestUtils.topicObserved(finalPaneEvent, () => true);
  await BrowserTestUtils.withNewTab(
    "about:preferences#general-migrate",
    async browser => {
      await finalPrefPaneLoaded;
      await browser.contentWindow.customElements.whenDefined(
        "migration-wizard"
      );
      let doc = browser.contentDocument;
      ok(
        !doc.getElementById("dataMigrationGroup"),
        "Should remove import entrypoint in prefs if disabled via policy."
      );
      ok(
        !doc.getElementById("migrationWizardDialog").open,
        "Should not have opened the migration wizard."
      );
    }
  );
});
