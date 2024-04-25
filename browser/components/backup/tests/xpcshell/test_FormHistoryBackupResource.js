/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { FormHistoryBackupResource } = ChromeUtils.importESModule(
  "resource:///modules/backup/FormHistoryBackupResource.sys.mjs"
);

/**
 * Tests that we can measure the Form History db in a profile directory.
 */
add_task(async function test_measure() {
  const EXPECTED_FORM_HISTORY_DB_SIZE = 500;

  Services.fog.testResetFOG();

  // Create resource files in temporary directory
  let tempDir = PathUtils.tempDir;
  let tempFormHistoryDBPath = PathUtils.join(tempDir, "formhistory.sqlite");
  await createKilobyteSizedFile(
    tempFormHistoryDBPath,
    EXPECTED_FORM_HISTORY_DB_SIZE
  );

  let formHistoryBackupResource = new FormHistoryBackupResource();
  await formHistoryBackupResource.measure(tempDir);

  let formHistoryMeasurement =
    Glean.browserBackup.formHistorySize.testGetValue();
  let scalars = TelemetryTestUtils.getProcessScalars("parent", false, false);

  // Compare glean vs telemetry measurements
  TelemetryTestUtils.assertScalar(
    scalars,
    "browser.backup.form_history_size",
    formHistoryMeasurement,
    "Glean and telemetry measurements for formhistory.sqlite should be equal"
  );

  // Compare glean measurements vs actual file sizes
  Assert.equal(
    formHistoryMeasurement,
    EXPECTED_FORM_HISTORY_DB_SIZE,
    "Should have collected the correct glean measurement for formhistory.sqlite"
  );

  await IOUtils.remove(tempFormHistoryDBPath);
});

/**
 * Test that the backup method correctly copies items from the profile directory
 * into the staging directory.
 */
add_task(async function test_backup() {
  let sandbox = sinon.createSandbox();

  let formHistoryBackupResource = new FormHistoryBackupResource();
  let sourcePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "FormHistoryBackupResource-source-test"
  );
  let stagingPath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "FormHistoryBackupResource-staging-test"
  );

  // Make sure this file exists in the source directory, otherwise
  // BackupResource will skip attempting to back it up.
  await createTestFiles(sourcePath, [{ path: "formhistory.sqlite" }]);

  // We have no need to test that Sqlite.sys.mjs's backup method is working -
  // this is something that is tested in Sqlite's own tests. We can just make
  // sure that it's being called using sinon. Unfortunately, we cannot do the
  // same thing with IOUtils.copy, as its methods are not stubbable.
  let fakeConnection = {
    backup: sandbox.stub().resolves(true),
    close: sandbox.stub().resolves(true),
  };
  sandbox.stub(Sqlite, "openConnection").returns(fakeConnection);

  let manifestEntry = await formHistoryBackupResource.backup(
    stagingPath,
    sourcePath
  );
  Assert.equal(
    manifestEntry,
    null,
    "FormHistoryBackupResource.backup should return null as its ManifestEntry"
  );

  // Next, we'll make sure that the Sqlite connection had `backup` called on it
  // with the right arguments.
  Assert.ok(
    fakeConnection.backup.calledOnce,
    "Called backup the expected number of times for all connections"
  );
  Assert.ok(
    fakeConnection.backup.calledWith(
      PathUtils.join(stagingPath, "formhistory.sqlite")
    ),
    "Called backup on the formhistory.sqlite Sqlite connection"
  );

  await maybeRemovePath(stagingPath);
  await maybeRemovePath(sourcePath);

  sandbox.restore();
});

/**
 * Test that the recover method correctly copies items from the recovery
 * directory into the destination profile directory.
 */
add_task(async function test_recover() {
  let formHistoryBackupResource = new FormHistoryBackupResource();
  let recoveryPath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "FormHistoryBackupResource-recovery-test"
  );
  let destProfilePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "FormHistoryBackupResource-test-profile"
  );

  const simpleCopyFiles = [{ path: "formhistory.sqlite" }];
  await createTestFiles(recoveryPath, simpleCopyFiles);

  // The backup method is expected to have returned a null ManifestEntry
  let postRecoveryEntry = await formHistoryBackupResource.recover(
    null /* manifestEntry */,
    recoveryPath,
    destProfilePath
  );
  Assert.equal(
    postRecoveryEntry,
    null,
    "FormHistoryBackupResource.recover should return null as its post " +
      "recovery entry"
  );

  await assertFilesExist(destProfilePath, simpleCopyFiles);

  await maybeRemovePath(recoveryPath);
  await maybeRemovePath(destProfilePath);
});
