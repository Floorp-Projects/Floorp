/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the MigrationWizard can be used to successfully migrate
 * using the InternalTestingProfileMigrator in a few scenarios.
 */
add_task(async function test_successful_migrations() {
  // Scenario 1: A single resource type is available.
  let migration = waitForTestMigration(
    [MigrationUtils.resourceTypes.BOOKMARKS],
    [MigrationUtils.resourceTypes.BOOKMARKS],
    InternalTestingProfileMigrator.testProfile
  );

  await withMigrationWizardDialog(async prefsWin => {
    let dialogBody = prefsWin.document.body;
    let wizard = dialogBody.querySelector("migration-wizard");
    let shadow = wizard.openOrClosedShadowRoot;
    let selector = shadow.querySelector("#browser-profile-selector");

    await new Promise(resolve => prefsWin.requestAnimationFrame(resolve));
    Assert.equal(shadow.activeElement, selector, "Selector should be focused.");

    let wizardDone = BrowserTestUtils.waitForEvent(
      wizard,
      "MigrationWizard:DoneMigration"
    );
    selectResourceTypesAndStartMigration(wizard, [
      MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.BOOKMARKS,
    ]);
    await migration;
    await wizardDone;

    let dialog = prefsWin.document.querySelector("#migrationWizardDialog");
    let doneButton = shadow.querySelector(
      "div[name='page-progress'] .done-button"
    );

    await new Promise(resolve => prefsWin.requestAnimationFrame(resolve));
    Assert.equal(
      shadow.activeElement,
      doneButton,
      "Done button should be focused."
    );

    let dialogClosed = BrowserTestUtils.waitForEvent(dialog, "close");
    doneButton.click();
    await dialogClosed;
    assertQuantitiesShown(wizard, [
      MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.BOOKMARKS,
    ]);
  });

  // We should make sure that the migration.time_to_produce_migrator_list
  // scalar was set, since we know that at least one migration wizard has
  // been opened.
  let scalars = TelemetryTestUtils.getProcessScalars("parent", false, false);
  Assert.ok(
    scalars["migration.time_to_produce_migrator_list"] > 0,
    "Non-zero scalar value recorded for migration.time_to_produce_migrator_list"
  );

  // Scenario 2: Several resource types are available, but only 1
  // is checked / expected.
  migration = waitForTestMigration(
    [
      MigrationUtils.resourceTypes.BOOKMARKS,
      MigrationUtils.resourceTypes.PASSWORDS,
    ],
    [MigrationUtils.resourceTypes.PASSWORDS],
    InternalTestingProfileMigrator.testProfile
  );

  await withMigrationWizardDialog(async prefsWin => {
    let dialogBody = prefsWin.document.body;
    let wizard = dialogBody.querySelector("migration-wizard");
    let shadow = wizard.openOrClosedShadowRoot;
    let selector = shadow.querySelector("#browser-profile-selector");

    await new Promise(resolve => prefsWin.requestAnimationFrame(resolve));
    Assert.equal(shadow.activeElement, selector, "Selector should be focused.");

    let wizardDone = BrowserTestUtils.waitForEvent(
      wizard,
      "MigrationWizard:DoneMigration"
    );
    selectResourceTypesAndStartMigration(wizard, [
      MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS,
    ]);
    await migration;
    await wizardDone;

    let dialog = prefsWin.document.querySelector("#migrationWizardDialog");
    let doneButton = shadow.querySelector(
      "div[name='page-progress'] .done-button"
    );

    await new Promise(resolve => prefsWin.requestAnimationFrame(resolve));
    Assert.equal(
      shadow.activeElement,
      doneButton,
      "Done button should be focused."
    );

    let dialogClosed = BrowserTestUtils.waitForEvent(dialog, "close");
    doneButton.click();
    await dialogClosed;
    assertQuantitiesShown(wizard, [
      MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS,
    ]);
  });

  // Scenario 3: Several resource types are available, all are checked.
  let allResourceTypeStrs = Object.values(
    MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES
  ).filter(resourceStr => {
    return !MigrationWizardConstants.PROFILE_RESET_ONLY_RESOURCE_TYPES[
      resourceStr
    ];
  });

  let allResourceTypes = allResourceTypeStrs.map(resourceTypeStr => {
    return MigrationUtils.resourceTypes[resourceTypeStr];
  });

  migration = waitForTestMigration(
    allResourceTypes,
    allResourceTypes,
    InternalTestingProfileMigrator.testProfile
  );

  await withMigrationWizardDialog(async prefsWin => {
    let dialogBody = prefsWin.document.body;
    let wizard = dialogBody.querySelector("migration-wizard");
    let shadow = wizard.openOrClosedShadowRoot;
    let selector = shadow.querySelector("#browser-profile-selector");

    await new Promise(resolve => prefsWin.requestAnimationFrame(resolve));
    Assert.equal(shadow.activeElement, selector, "Selector should be focused.");

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
    InternalTestingProfileMigrator.testProfile
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
    await wizardDone;

    let dialog = prefsWin.document.querySelector("#migrationWizardDialog");
    let shadow = wizard.openOrClosedShadowRoot;
    let doneButton = shadow.querySelector(
      "div[name='page-progress'] .done-button"
    );

    await new Promise(resolve => prefsWin.requestAnimationFrame(resolve));
    Assert.equal(
      shadow.activeElement,
      doneButton,
      "Done button should be focused."
    );

    assertQuantitiesShown(wizard, [
      MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.BOOKMARKS,
    ]);

    let dialogClosed = BrowserTestUtils.waitForEvent(dialog, "close");
    doneButton.click();
    await dialogClosed;
  });
});
