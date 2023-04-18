/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PasswordFileMigrator } = ChromeUtils.importESModule(
  "resource:///modules/FileMigrators.sys.mjs"
);
const { LoginCSVImport } = ChromeUtils.import(
  "resource://gre/modules/LoginCSVImport.jsm"
);
const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);
const { MigrationWizardConstants } = ChromeUtils.importESModule(
  "chrome://browser/content/migration/migration-wizard-constants.mjs"
);

/**
 * Tests that the PasswordFileMigrator properly subclasses FileMigratorBase
 * and delegates to the LoginCSVImport module.
 */
add_task(async function test_PasswordFileMigrator() {
  Services.prefs.setBoolPref("signon.management.page.fileImport.enabled", true);
  let sandbox = sinon.createSandbox();
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("signon.management.page.fileImport.enabled");
    sandbox.restore();
  });

  let migrator = new PasswordFileMigrator();
  Assert.ok(
    migrator.constructor.key,
    "PasswordFileMigrator implements static getter 'key'"
  );
  Assert.ok(
    migrator.constructor.displayNameL10nID,
    "PasswordFileMigrator implements static getter 'displayNameL10nID'"
  );
  Assert.ok(
    await migrator.getFilePickerConfig(),
    "PasswordFileMigrator returns something for getFilePickerConfig()"
  );
  Assert.ok(
    migrator.displayedResourceTypes,
    "PasswordFileMigrator returns something for displayedResourceTypes"
  );
  Assert.ok(migrator.enabled, "PasswordFileMigrator is enabled.");

  const IMPORT_SUMMARY = [
    {
      result: "added",
    },
    {
      result: "added",
    },
    {
      result: "modified",
    },
  ];
  const EXPECTED_SUCCESS_STATE = {
    [MigrationWizardConstants.DISPLAYED_FILE_RESOURCE_TYPES.PASSWORDS_NEW]:
      "2 added",
    [MigrationWizardConstants.DISPLAYED_FILE_RESOURCE_TYPES.PASSWORDS_UPDATED]:
      "1 updated",
  };
  const FAKE_PATH = "some/fake/path.csv";

  let importFromCSVStub = sandbox
    .stub(LoginCSVImport, "importFromCSV")
    .callsFake(somePath => {
      Assert.equal(somePath, FAKE_PATH, "Got expected path");
      return Promise.resolve(IMPORT_SUMMARY);
    });
  let result = await migrator.migrate(FAKE_PATH);

  Assert.ok(importFromCSVStub.called, "The stub should have been called.");
  Assert.deepEqual(
    result,
    EXPECTED_SUCCESS_STATE,
    "Got back the expected success state."
  );
});
