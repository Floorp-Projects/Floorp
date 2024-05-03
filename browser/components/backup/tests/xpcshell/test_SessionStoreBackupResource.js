/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { SessionStoreBackupResource } = ChromeUtils.importESModule(
  "resource:///modules/backup/SessionStoreBackupResource.sys.mjs"
);
const { SessionStore } = ChromeUtils.importESModule(
  "resource:///modules/sessionstore/SessionStore.sys.mjs"
);

/**
 * Tests that we can measure the Session Store JSON and backups directory.
 */
add_task(async function test_measure() {
  const EXPECTED_KILOBYTES_FOR_BACKUPS_DIR = 1000;
  Services.fog.testResetFOG();

  // Create the sessionstore-backups directory.
  let tempDir = PathUtils.tempDir;
  let sessionStoreBackupsPath = PathUtils.join(
    tempDir,
    "sessionstore-backups",
    "restore.jsonlz4"
  );
  await createKilobyteSizedFile(
    sessionStoreBackupsPath,
    EXPECTED_KILOBYTES_FOR_BACKUPS_DIR
  );

  let sessionStoreBackupResource = new SessionStoreBackupResource();
  await sessionStoreBackupResource.measure(tempDir);

  let sessionStoreBackupsDirectoryMeasurement =
    Glean.browserBackup.sessionStoreBackupsDirectorySize.testGetValue();
  let sessionStoreMeasurement =
    Glean.browserBackup.sessionStoreSize.testGetValue();
  let scalars = TelemetryTestUtils.getProcessScalars("parent", false, false);

  // Compare glean vs telemetry measurements
  TelemetryTestUtils.assertScalar(
    scalars,
    "browser.backup.session_store_backups_directory_size",
    sessionStoreBackupsDirectoryMeasurement,
    "Glean and telemetry measurements for session store backups directory should be equal"
  );
  TelemetryTestUtils.assertScalar(
    scalars,
    "browser.backup.session_store_size",
    sessionStoreMeasurement,
    "Glean and telemetry measurements for session store should be equal"
  );

  // Compare glean measurements vs actual file sizes
  Assert.equal(
    sessionStoreBackupsDirectoryMeasurement,
    EXPECTED_KILOBYTES_FOR_BACKUPS_DIR,
    "Should have collected the correct glean measurement for the sessionstore-backups directory"
  );

  // Session store measurement is from `getCurrentState`, so exact size is unknown.
  Assert.greater(
    sessionStoreMeasurement,
    0,
    "Should have collected a measurement for the session store"
  );

  await IOUtils.remove(sessionStoreBackupsPath);
});

/**
 * Test that the backup method correctly copies items from the profile directory
 * into the staging directory.
 */
add_task(async function test_backup() {
  let sandbox = sinon.createSandbox();

  let sessionStoreBackupResource = new SessionStoreBackupResource();
  let sourcePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "SessionStoreBackupResource-source-test"
  );
  let stagingPath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "SessionStoreBackupResource-staging-test"
  );

  const simpleCopyFiles = [
    { path: ["sessionstore-backups", "test-sessionstore-backup.jsonlz4"] },
    { path: ["sessionstore-backups", "test-sessionstore-recovery.baklz4"] },
  ];
  await createTestFiles(sourcePath, simpleCopyFiles);

  let sessionStoreState = SessionStore.getCurrentState(true);
  let manifestEntry = await sessionStoreBackupResource.backup(
    stagingPath,
    sourcePath
  );
  Assert.equal(
    manifestEntry,
    null,
    "SessionStoreBackupResource.backup should return null as its ManifestEntry"
  );

  /**
   * We don't expect the actual file sessionstore.jsonlz4 to exist in the profile directory before calling the backup method.
   * Instead, verify that it is created by the backup method and exists in the staging folder right after.
   */
  await assertFilesExist(stagingPath, [
    ...simpleCopyFiles,
    { path: "sessionstore.jsonlz4" },
  ]);

  /**
   * Do a deep comparison between the recorded session state before backup and the file made in the staging folder
   * to verify that information about session state was correctly written for backup.
   */
  let sessionStoreStateStaged = await IOUtils.readJSON(
    PathUtils.join(stagingPath, "sessionstore.jsonlz4"),
    { decompress: true }
  );

  /**
   * These timestamps might be slightly different from one another, so we'll exclude
   * them from the comparison.
   */
  delete sessionStoreStateStaged.session.lastUpdate;
  delete sessionStoreState.session.lastUpdate;
  Assert.deepEqual(
    sessionStoreStateStaged,
    sessionStoreState,
    "sessionstore.jsonlz4 in the staging folder matches the recorded session state"
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
  let sessionStoreBackupResource = new SessionStoreBackupResource();
  let recoveryPath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "SessionStoreBackupResource-recovery-test"
  );
  let destProfilePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "SessionStoreBackupResource-test-profile"
  );

  const simpleCopyFiles = [
    { path: ["sessionstore-backups", "test-sessionstore-backup.jsonlz4"] },
    { path: ["sessionstore-backups", "test-sessionstore-recovery.baklz4"] },
  ];
  await createTestFiles(recoveryPath, simpleCopyFiles);

  // We backup a copy of sessionstore.jsonlz4, so ensure it exists in the recovery path
  let sessionStoreState = SessionStore.getCurrentState(true);
  let sessionStoreBackupPath = PathUtils.join(
    recoveryPath,
    "sessionstore.jsonlz4"
  );
  await IOUtils.writeJSON(sessionStoreBackupPath, sessionStoreState, {
    compress: true,
  });

  // The backup method is expected to have returned a null ManifestEntry
  let postRecoveryEntry = await sessionStoreBackupResource.recover(
    null /* manifestEntry */,
    recoveryPath,
    destProfilePath
  );
  Assert.equal(
    postRecoveryEntry,
    null,
    "SessionStoreBackupResource.recover should return null as its post recovery entry"
  );

  await assertFilesExist(destProfilePath, [
    ...simpleCopyFiles,
    { path: "sessionstore.jsonlz4" },
  ]);

  let sessionStateCopied = await IOUtils.readJSON(
    PathUtils.join(destProfilePath, "sessionstore.jsonlz4"),
    { decompress: true }
  );

  /**
   * These timestamps might be slightly different from one another, so we'll exclude
   * them from the comparison.
   */
  delete sessionStateCopied.session.lastUpdate;
  delete sessionStoreState.session.lastUpdate;
  Assert.deepEqual(
    sessionStateCopied,
    sessionStoreState,
    "sessionstore.jsonlz4 in the destination profile folder matches the backed up session state"
  );

  await maybeRemovePath(recoveryPath);
  await maybeRemovePath(destProfilePath);
});
