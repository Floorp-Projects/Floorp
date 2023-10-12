/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ChromeProfileMigrator } = ChromeUtils.importESModule(
  "resource:///modules/ChromeProfileMigrator.sys.mjs"
);

/**
 * Tests that if the migration wizard is opened when the
 * MOZ_UNINSTALLER_PROFILE_REFRESH environment variable is defined,
 * that the migration.uninstaller_profile_refresh scalar is set,
 * and the environment variable is cleared.
 */
add_task(async function test_uninstaller_migration() {
  if (AppConstants.platform != "win") {
    return;
  }

  Services.env.set("MOZ_UNINSTALLER_PROFILE_REFRESH", "1");
  let wizardPromise = BrowserTestUtils.domWindowOpened();
  // Opening the migration wizard this way is a blocking function, so
  // we delegate it to a runnable.
  executeSoon(() => {
    MigrationUtils.showMigrationWizard(null, { isStartupMigration: true });
  });
  let wizardWin = await wizardPromise;

  await BrowserTestUtils.waitForEvent(wizardWin, "MigrationWizard:Ready");

  let scalars = TelemetryTestUtils.getProcessScalars("parent", false, true);
  TelemetryTestUtils.assertScalar(
    scalars,
    "migration.uninstaller_profile_refresh",
    1
  );

  Assert.equal(
    Services.env.get("MOZ_UNINSTALLER_PROFILE_REFRESH"),
    "",
    "Cleared MOZ_UNINSTALLER_PROFILE_REFRESH environment variable."
  );
  await BrowserTestUtils.closeWindow(wizardWin);
});

/**
 * Tests that we populate the migration.discovered_migrators keyed scalar
 * with a count of discovered browsers and profiles.
 */
add_task(async function test_discovered_migrators_keyed_scalar() {
  Services.telemetry.clearScalars();

  let sandbox = sinon.createSandbox();
  registerCleanupFunction(() => {
    sandbox.restore();
  });

  // We'll pretend that this system only has the
  // InternalTestingProfileMigrator and ChromeProfileMigrator around to
  // start
  sandbox.stub(MigrationUtils, "availableMigratorKeys").get(() => {
    return ["internal-testing", "chrome"];
  });

  // The InternalTestingProfileMigrator by default returns a single profile
  // from `getSourceProfiles`, and now we'll just prepare the
  // ChromeProfileMigrator to return two fake profiles.
  sandbox.stub(ChromeProfileMigrator.prototype, "getSourceProfiles").resolves([
    { id: "chrome-test-1", name: "Chrome test profile 1" },
    { id: "chrome-test-2", name: "Chrome test profile 2" },
  ]);

  sandbox
    .stub(ChromeProfileMigrator.prototype, "hasPermissions")
    .resolves(true);

  // We also need to ensure that the ChromeProfileMigrator actually has
  // some resources to migrate, otherwise it won't get listed.
  sandbox
    .stub(ChromeProfileMigrator.prototype, "getResources")
    .callsFake(() => {
      return Promise.resolve(
        Object.values(MigrationUtils.resourceType).map(resourceType => {
          return {
            type: resourceType,
            migrate: () => {},
          };
        })
      );
    });

  await withMigrationWizardDialog(async () => {
    let scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
    TelemetryTestUtils.assertKeyedScalar(
      scalars,
      "migration.discovered_migrators",
      InternalTestingProfileMigrator.key,
      1
    );
    TelemetryTestUtils.assertKeyedScalar(
      scalars,
      "migration.discovered_migrators",
      ChromeProfileMigrator.key,
      2
    );
  });

  // Now, reset, and we'll try the case where a migrator returns `null` from
  // `getSourceProfiles` using the InternalTestingProfileMigrator again.
  sandbox.restore();

  sandbox = sinon.createSandbox();

  sandbox.stub(MigrationUtils, "availableMigratorKeys").get(() => {
    return ["internal-testing"];
  });

  sandbox
    .stub(ChromeProfileMigrator.prototype, "getSourceProfiles")
    .resolves(null);

  await withMigrationWizardDialog(async () => {
    let scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
    TelemetryTestUtils.assertKeyedScalar(
      scalars,
      "migration.discovered_migrators",
      InternalTestingProfileMigrator.key,
      1
    );
  });

  sandbox.restore();
});

/**
 * Tests that we write to the FX_MIGRATION_ERRORS histogram when a
 * resource fails to migrate properly.
 */
add_task(async function test_fx_migration_errors() {
  let migration = waitForTestMigration(
    [
      MigrationUtils.resourceTypes.BOOKMARKS,
      MigrationUtils.resourceTypes.PASSWORDS,
    ],
    [
      MigrationUtils.resourceTypes.BOOKMARKS,
      MigrationUtils.resourceTypes.PASSWORDS,
    ],
    InternalTestingProfileMigrator.testProfile,
    [MigrationUtils.resourceTypes.PASSWORDS]
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
      MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS,
    ]);
    await migration;
    await wizardDone;

    assertQuantitiesShown(
      wizard,
      [
        MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.BOOKMARKS,
        MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS,
      ],
      [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS]
    );
  });
});
