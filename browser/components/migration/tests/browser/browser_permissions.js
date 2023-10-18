/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.migrate.chrome.get_permissions.enabled", true]],
  });
});

/**
 * Tests that the migration wizard can request permission from
 * the user to read from other browser data directories when
 * explicit permission needs to be granted.
 *
 * This can occur when, for example, Firefox is installed as a
 * Snap on Ubuntu Linux. In this state, Firefox does not have
 * direct read access to other browser's data directories (although)
 * it can tell if they exist. For Chromium-based browsers, this
 * means we cannot tell what profiles nor resources are available
 * for Chromium-based browsers without read permissions.
 *
 * Note that the Safari migrator is not tested here, as it has
 * its own special permission flow. This is because we can
 * determine what resources Safari has before requiring permissions,
 * and (as of this writing) Safari does not support multiple
 * user profiles.
 */
add_task(async function test_permissions() {
  Services.telemetry.clearEvents();

  let sandbox = sinon.createSandbox();
  registerCleanupFunction(() => {
    sandbox.restore();
  });

  sandbox
    .stub(InternalTestingProfileMigrator.prototype, "canGetPermissions")
    .resolves("/some/path");

  let hasPermissionsStub = sandbox
    .stub(InternalTestingProfileMigrator.prototype, "hasPermissions")
    .resolves(false);

  let testingMigrator = await MigrationUtils.getMigrator(
    InternalTestingProfileMigrator.key
  );
  Assert.ok(
    testingMigrator,
    "Got migrator, even though we don't yet have permission to read its resources."
  );

  sandbox.stub(testingMigrator, "getPermissions").callsFake(async () => {
    testingMigrator.flushResourceCache();
    hasPermissionsStub.resolves(true);
    return Promise.resolve(true);
  });

  let getResourcesStub = sandbox
    .stub(testingMigrator, "getResources")
    .resolves([]);

  let migration = waitForTestMigration(
    [MigrationUtils.resourceTypes.BOOKMARKS],
    [MigrationUtils.resourceTypes.BOOKMARKS],
    InternalTestingProfileMigrator.testProfile
  );

  await withMigrationWizardDialog(async prefsWin => {
    let dialogBody = prefsWin.document.body;
    let wizard = dialogBody.querySelector("migration-wizard");
    let shadow = wizard.openOrClosedShadowRoot;

    // Clear out any pre-existing events events have been logged
    Services.telemetry.clearEvents();
    TelemetryTestUtils.assertNumberOfEvents(0);

    let panelItem = shadow.querySelector(
      `panel-item[key="${InternalTestingProfileMigrator.key}"]`
    );
    panelItem.click();

    let resourceList = shadow.querySelector(".resource-selection-details");
    Assert.ok(
      BrowserTestUtils.is_hidden(resourceList),
      "Resources list is hidden."
    );
    let importButton = shadow.querySelector("#import");
    Assert.ok(
      BrowserTestUtils.is_hidden(importButton),
      "Import button hidden."
    );
    let noPermissionsMessage = shadow.querySelector(".no-permissions-message");
    Assert.ok(
      BrowserTestUtils.is_visible(noPermissionsMessage),
      "No permissions message shown."
    );
    let getPermissionButton = shadow.querySelector("#get-permissions");
    Assert.ok(
      BrowserTestUtils.is_visible(getPermissionButton),
      "Get permissions button shown."
    );

    // Now put the permissions functions back into their default
    // state - which is the "permission granted" state.
    getResourcesStub.restore();
    hasPermissionsStub.restore();

    let refreshDone = BrowserTestUtils.waitForEvent(
      wizard,
      "MigrationWizard:Ready"
    );

    getPermissionButton.click();

    await refreshDone;
    Assert.ok(true, "Refreshed migrator list.");

    let wizardDone = BrowserTestUtils.waitForEvent(
      wizard,
      "MigrationWizard:DoneMigration"
    );
    selectResourceTypesAndStartMigration(wizard, [
      MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.BOOKMARKS,
    ]);
    await migration;
    await wizardDone;

    assertQuantitiesShown(wizard, [
      MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.BOOKMARKS,
    ]);

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
  });

  TelemetryTestUtils.assertEvents(
    [
      {
        category: "browser.migration",
        method: "linux_perms",
        object: "wizard",
        value: null,
        extra: {
          migrator_key: InternalTestingProfileMigrator.key,
        },
      },
    ],
    {
      category: "browser.migration",
      method: "linux_perms",
      object: "wizard",
    }
  );
});
