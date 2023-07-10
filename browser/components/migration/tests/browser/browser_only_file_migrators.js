/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the NO_BROWSERS_FOUND page has a button to redirect to the
 * selection page when only file migrators are found.
 */
add_task(async function test_only_file_migrators() {
  await SpecialPowers.pushPrefEnv({
    set: [["signon.management.page.fileImport.enabled", true]],
  });

  let sandbox = sinon.createSandbox();
  registerCleanupFunction(() => {
    sandbox.restore();
  });

  sandbox.stub(MigrationUtils, "availableMigratorKeys").get(() => {
    return [];
  });

  await withMigrationWizardDialog(async prefsWin => {
    let dialog = prefsWin.document.querySelector("#migrationWizardDialog");
    let wizard = dialog.querySelector("migration-wizard");
    let shadow = wizard.openOrClosedShadowRoot;
    let deck = shadow.querySelector("#wizard-deck");

    await BrowserTestUtils.waitForMutationCondition(
      deck,
      { attributeFilter: ["selected-view"] },
      () => {
        return (
          deck.getAttribute("selected-view") ==
          "page-" + MigrationWizardConstants.PAGES.NO_BROWSERS_FOUND
        );
      }
    );

    let chooseImportFileButton = shadow.querySelector(
      "#choose-import-from-file"
    );

    let changedToSelectionPage = BrowserTestUtils.waitForMutationCondition(
      deck,
      { attributeFilter: ["selected-view"] },
      () => {
        return (
          deck.getAttribute("selected-view") ==
          "page-" + MigrationWizardConstants.PAGES.SELECTION
        );
      }
    );
    chooseImportFileButton.click();
    await changedToSelectionPage;

    // No browser migrators should be listed.
    let browserMigratorItems = shadow.querySelectorAll(
      `panel-item[type="${MigrationWizardConstants.MIGRATOR_TYPES.BROWSER}"]`
    );
    Assert.ok(!browserMigratorItems.length, "No browser migrators listed.");

    // Check to make sure there's at least one file migrator listed.
    let fileMigratorItems = shadow.querySelectorAll(
      `panel-item[type="${MigrationWizardConstants.MIGRATOR_TYPES.FILE}"]`
    );

    Assert.ok(!!fileMigratorItems.length, "Listed at least one file migrator.");
  });
});
