/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);
const { InternalTestingProfileMigrator } = ChromeUtils.importESModule(
  "resource:///modules/InternalTestingProfileMigrator.sys.mjs"
);

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
 * We'll have this be our magic number of quantities of various imports.
 * We will use Sinon to prepare MigrationUtils to presume that this was
 * how many of each quantity-supported resource type was imported.
 */
const EXPECTED_QUANTITY = 123;

/**
 * A helper function that prepares the InternalTestingProfileMigrator
 * with some set of fake available resources, and resolves a Promise
 * when the InternalTestingProfileMigrator is used for a migration.
 *
 * @param {number[]} availableResourceTypes
 *   An array of resource types from MigrationUtils.resourcesTypes.
 *   A single MigrationResource will be created per type, with a
 *   no-op migrate function.
 * @param {number[]} expectedResourceTypes
 *   An array of resource types from MigrationUtils.resourceTypes.
 *   These are the resource types that are expected to be passed
 *   to the InternalTestingProfileMigrator.migrate function.
 * @param {object|string} expectedProfile
 *   The profile object or string that is expected to be passed
 *   to the InternalTestingProfileMigrator.migrate function.
 * @returns {Promise<undefined>}
 */
async function waitForTestMigration(
  availableResourceTypes,
  expectedResourceTypes,
  expectedProfile
) {
  let sandbox = sinon.createSandbox();

  // Fake out the getResources method of the migrator so that we return
  // a single fake MigratorResource per availableResourceType.
  sandbox
    .stub(InternalTestingProfileMigrator.prototype, "getResources")
    .callsFake(() => {
      return Promise.resolve(
        availableResourceTypes.map(resourceType => {
          return {
            type: resourceType,
            migrate: () => {},
          };
        })
      );
    });

  sandbox.stub(MigrationUtils, "_importQuantities").value({
    bookmarks: EXPECTED_QUANTITY,
    history: EXPECTED_QUANTITY,
    logins: EXPECTED_QUANTITY,
  });

  // Fake out the migrate method of the migrator and assert that the
  // next time it's called, its arguments match our expectations.
  return new Promise(resolve => {
    sandbox
      .stub(InternalTestingProfileMigrator.prototype, "migrate")
      .callsFake((aResourceTypes, aStartup, aProfile, aProgressCallback) => {
        Assert.ok(
          !aStartup,
          "Migrator should not have been called as a startup migration."
        );

        let bitMask = 0;
        for (let resourceType of expectedResourceTypes) {
          bitMask |= resourceType;
        }

        Assert.deepEqual(
          aResourceTypes,
          bitMask,
          "Got the expected resource types"
        );
        Assert.deepEqual(
          aProfile,
          expectedProfile,
          "Got the expected profile object"
        );

        for (let resourceType of expectedResourceTypes) {
          aProgressCallback(resourceType);
        }
        Services.obs.notifyObservers(null, "Migration:Ended");
        resolve();
      });
  }).finally(async () => {
    sandbox.restore();

    // MigratorBase caches resources fetched by the getResources method
    // as a performance optimization. In order to allow different tests
    // to have different available resources, we call into a special
    // method of InternalTestingProfileMigrator that clears that
    // cache.
    let migrator = await MigrationUtils.getMigrator(
      InternalTestingProfileMigrator.key
    );
    migrator.flushResourceCache();
  });
}

/**
 * Takes a MigrationWizard element and chooses the
 * InternalTestingProfileMigrator as the browser to migrate from. Then, it
 * checks the checkboxes associated with the selectedResourceTypes and
 * unchecks the rest before clicking the "Import" button.
 *
 * @param {Element} wizard
 *   The MigrationWizard element.
 * @param {string[]} selectedResourceTypes
 *   An array of resource type strings from
 *   MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.
 */
async function selectResourceTypesAndStartMigration(
  wizard,
  selectedResourceTypes
) {
  let shadow = wizard.openOrClosedShadowRoot;

  // First, select the InternalTestingProfileMigrator browser.
  let selector = shadow.querySelector("#browser-profile-selector");
  selector.click();

  await new Promise(resolve => {
    wizard
      .querySelector("panel-list")
      .addEventListener("shown", resolve, { once: true });
  });

  let panelItem = wizard.querySelector(
    `panel-item[key="${InternalTestingProfileMigrator.key}"]`
  );
  panelItem.click();

  // And then check the right checkboxes for the resource types.
  let resourceTypeList = shadow.querySelector("#resource-type-list");
  for (let resourceType in MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES) {
    let node = resourceTypeList.querySelector(
      `label[data-resource-type="${resourceType}"]`
    );
    node.control.checked = selectedResourceTypes.includes(resourceType);
  }

  let importButton = shadow.querySelector("#import");
  importButton.click();
}

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
