/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");
const { InternalTestingProfileMigrator } = ChromeUtils.importESModule(
  "resource:///modules/InternalTestingProfileMigrator.sys.mjs"
);

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

  // Fake out the migrate method of the migrator and assert that the
  // next time it's called, its arguments match our expectations.
  return new Promise(resolve => {
    sandbox
      .stub(InternalTestingProfileMigrator.prototype, "migrate")
      .callsFake((aResourceTypes, aStartup, aProfile) => {
        Assert.ok(
          !aStartup,
          "Migrator should not have been called as a startup migration."
        );
        Assert.deepEqual(
          aResourceTypes,
          expectedResourceTypes,
          "Got the expected resource types"
        );
        Assert.deepEqual(
          aProfile,
          expectedProfile,
          "Got the expected profile object"
        );
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
function selectResourceTypesAndStartMigration(wizard, selectedResourceTypes) {
  let shadow = wizard.openOrClosedShadowRoot;

  // First, select the InternalTestingProfileMigrator browser.
  let selector = shadow.querySelector("#browser-profile-selector");
  selector.value = InternalTestingProfileMigrator.key;
  // Apparently we have to dispatch our own "change" events for <select>
  // dropdowns.
  selector.dispatchEvent(new CustomEvent("change", { bubbles: true }));

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

  await withMigrationWizardSubdialog(async subdialogWin => {
    let dialogBody = subdialogWin.document.body;
    let wizard = dialogBody.querySelector("#wizard");
    let wizardDone = BrowserTestUtils.waitForEvent(
      wizard,
      "MigrationWizard:DoneMigration"
    );
    selectResourceTypesAndStartMigration(wizard, [
      MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.BOOKMARKS,
    ]);
    await migration;
    await wizardDone;
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

  await withMigrationWizardSubdialog(async subdialogWin => {
    let dialogBody = subdialogWin.document.body;
    let wizard = dialogBody.querySelector("#wizard");
    let wizardDone = BrowserTestUtils.waitForEvent(
      wizard,
      "MigrationWizard:DoneMigration"
    );
    selectResourceTypesAndStartMigration(wizard, [
      MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS,
    ]);
    await migration;
    await wizardDone;
  });
});
