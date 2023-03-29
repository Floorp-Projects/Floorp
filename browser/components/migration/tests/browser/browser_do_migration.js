/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * These are the resource types that currently display their import success
 * message with a quantity.
 */
const RESOURCE_TYPES_WITH_QUANTITIES = [
  MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.BOOKMARKS,
  MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.HISTORY,
  MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS,
  MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.FORMDATA,
];

/**
 * Assert that the resource types passed in expectedResourceTypes are
 * showing a success state after a migration, and if they are part of
 * the RESOURCE_TYPES_WITH_QUANTITIES group, that they're showing the
 * EXPECTED_QUANTITY magic number in their success message. Otherwise,
 * we (currently) check that they show the empty string.
 *
 * @param {Element} wizard
 *   The MigrationWizard element.
 * @param {string[]} expectedResourceTypes
 *   An array of resource type strings from
 *   MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.
 */
function assertQuantitiesShown(wizard, expectedResourceTypes) {
  let shadow = wizard.openOrClosedShadowRoot;

  // Make sure that we're showing the progress page first.
  let deck = shadow.querySelector("#wizard-deck");
  Assert.equal(
    deck.selectedViewName,
    `page-${MigrationWizardConstants.PAGES.PROGRESS}`
  );

  // Go through each displayed resource and make sure that only the
  // ones that are expected are shown, and are showing the right
  // success message.

  let progressGroups = shadow.querySelectorAll(".resource-progress-group");
  for (let progressGroup of progressGroups) {
    if (expectedResourceTypes.includes(progressGroup.dataset.resourceType)) {
      let progressIcon = progressGroup.querySelector(".progress-icon");
      let successText = progressGroup.querySelector(".success-text")
        .textContent;

      Assert.ok(
        progressIcon.classList.contains("completed"),
        "Should be showing completed state."
      );

      if (
        RESOURCE_TYPES_WITH_QUANTITIES.includes(
          progressGroup.dataset.resourceType
        )
      ) {
        if (
          progressGroup.dataset.resourceType ==
          MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.HISTORY
        ) {
          // HISTORY is a special case that doesn't show the number of imported
          // history entries, but instead shows the maximum number of days of history
          // that might have been imported.
          Assert.notEqual(
            successText.indexOf(MigrationUtils.HISTORY_MAX_AGE_IN_DAYS),
            -1,
            `Found expected maximum number of days of history: ${successText}`
          );
        } else if (
          progressGroup.dataset.resourceType ==
          MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.FORMDATA
        ) {
          // FORMDATA is another special case, because we simply show "Form history" as
          // the success string, rather than a particular quantity.
          Assert.equal(
            successText,
            "Form history",
            `Found expected form data string: ${successText}`
          );
        } else {
          Assert.notEqual(
            successText.indexOf(EXPECTED_QUANTITY),
            -1,
            `Found expected quantity in success string: ${successText}`
          );
        }
      } else {
        // If you've found yourself here, and this is failing, it's probably because you've
        // updated MigrationWizardParent.#getStringForImportQuantity to return a string for
        // a resource type that's not in RESOURCE_TYPES_WITH_QUANTITIES, and you'll need
        // to modify this function to check for that string.
        Assert.equal(
          successText,
          "",
          "Expected the empty string if the resource type " +
            "isn't in RESOURCE_TYPES_WITH_QUANTITIES"
        );
      }
    } else {
      Assert.ok(
        BrowserTestUtils.is_hidden(progressGroup),
        `Resource progress group for ${progressGroup.dataset.resourceType}` +
          ` should be hidden.`
      );
    }
  }
}

/**
 * Tests that the MigrationWizard can be used to successfully migrate
 * using the InternalTestingProfileMigrator in a few scenarios.
 */
add_task(async function test_successful_migrations() {
  // Scenario 1: A single resource type is available.
  let migration = waitForTestMigration(
    [MigrationUtils.resourceTypes.BOOKMARKS],
    [MigrationUtils.resourceTypes.BOOKMARKS],
    null
  );

  await withMigrationWizardDialog(async prefsWin => {
    let dialogBody = prefsWin.document.body;
    let wizard = dialogBody.querySelector("migration-wizard");
    let wizardDone = BrowserTestUtils.waitForEvent(
      wizard,
      "MigrationWizard:DoneMigration"
    );
    selectResourceTypesAndStartMigration(wizard, [
      MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.BOOKMARKS,
    ]);
    await migration;

    let dialog = prefsWin.document.querySelector("#migrationWizardDialog");
    let shadow = wizard.openOrClosedShadowRoot;
    let doneButton = shadow.querySelector("#done-button");
    let dialogClosed = BrowserTestUtils.waitForEvent(dialog, "close");

    doneButton.click();
    await dialogClosed;
    await wizardDone;
    assertQuantitiesShown(wizard, [
      MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.BOOKMARKS,
    ]);
  });

  // Scenario 2: Several resource types are available, but only 1
  // is checked / expected.
  migration = waitForTestMigration(
    [
      MigrationUtils.resourceTypes.BOOKMARKS,
      MigrationUtils.resourceTypes.PASSWORDS,
    ],
    [MigrationUtils.resourceTypes.PASSWORDS],
    null
  );

  await withMigrationWizardDialog(async prefsWin => {
    let dialogBody = prefsWin.document.body;
    let wizard = dialogBody.querySelector("migration-wizard");
    let wizardDone = BrowserTestUtils.waitForEvent(
      wizard,
      "MigrationWizard:DoneMigration"
    );
    selectResourceTypesAndStartMigration(wizard, [
      MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS,
    ]);
    await migration;

    let dialog = prefsWin.document.querySelector("#migrationWizardDialog");
    let shadow = wizard.openOrClosedShadowRoot;
    let doneButton = shadow.querySelector("#done-button");
    let dialogClosed = BrowserTestUtils.waitForEvent(dialog, "close");

    doneButton.click();
    await dialogClosed;
    await wizardDone;
    assertQuantitiesShown(wizard, [
      MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS,
    ]);
  });

  // Scenario 3: Several resource types are available, all are checked.
  let allResourceTypeStrs = Object.values(
    MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES
  );
  let allResourceTypes = allResourceTypeStrs.map(resourceTypeStr => {
    return MigrationUtils.resourceTypes[resourceTypeStr];
  });

  migration = waitForTestMigration(allResourceTypes, allResourceTypes, null);

  await withMigrationWizardDialog(async prefsWin => {
    let dialogBody = prefsWin.document.body;
    let wizard = dialogBody.querySelector("migration-wizard");
    let wizardDone = BrowserTestUtils.waitForEvent(
      wizard,
      "MigrationWizard:DoneMigration"
    );
    selectResourceTypesAndStartMigration(wizard, allResourceTypeStrs);
    await migration;
    await wizardDone;
    assertQuantitiesShown(wizard, allResourceTypeStrs);
  });
});

/**
 * Tests that if somehow the Migration Wizard requests to import a
 * resource type that the migrator doesn't have the ability to import,
 * that it's ignored and the migration completes normally.
 */
add_task(async function test_invalid_resource_type() {
  let migration = waitForTestMigration(
    [MigrationUtils.resourceTypes.BOOKMARKS],
    [MigrationUtils.resourceTypes.BOOKMARKS],
    null
  );

  await withMigrationWizardDialog(async prefsWin => {
    let dialogBody = prefsWin.document.body;
    let wizard = dialogBody.querySelector("migration-wizard");
    let wizardDone = BrowserTestUtils.waitForEvent(
      wizard,
      "MigrationWizard:DoneMigration"
    );

    // The Migration Wizard _shouldn't_ display anything except BOOKMARKS,
    // since that's the only resource type that the selected migrator is
    // supposed to currently support, but we'll check the other checkboxes
    // even though they're hidden just to see what happens.
    selectResourceTypesAndStartMigration(wizard, [
      MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.BOOKMARKS,
      MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS,
      MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.HISTORY,
      MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.FORMDATA,
    ]);
    await migration;

    let dialog = prefsWin.document.querySelector("#migrationWizardDialog");
    let shadow = wizard.openOrClosedShadowRoot;
    let doneButton = shadow.querySelector("#done-button");
    let dialogClosed = BrowserTestUtils.waitForEvent(dialog, "close");

    doneButton.click();
    await dialogClosed;
    await wizardDone;
    assertQuantitiesShown(wizard, [
      MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.BOOKMARKS,
    ]);
  });
});
