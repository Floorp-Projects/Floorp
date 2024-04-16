/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { CookiesBackupResource } = ChromeUtils.importESModule(
  "resource:///modules/backup/CookiesBackupResource.sys.mjs"
);

const { FormHistoryBackupResource } = ChromeUtils.importESModule(
  "resource:///modules/backup/FormHistoryBackupResource.sys.mjs"
);

const { SessionStoreBackupResource } = ChromeUtils.importESModule(
  "resource:///modules/backup/SessionStoreBackupResource.sys.mjs"
);

add_setup(() => {
  // FOG needs to be initialized in order for data to flow.
  Services.fog.initializeFOG();
  Services.telemetry.clearScalars();
});

/**
 * Tests that calling `BackupService.takeMeasurements` will call the measure
 * method of all registered BackupResource classes.
 */
add_task(async function test_takeMeasurements() {
  let sandbox = sinon.createSandbox();
  sandbox.stub(FakeBackupResource1.prototype, "measure").resolves();
  sandbox
    .stub(FakeBackupResource2.prototype, "measure")
    .rejects(new Error("Some failure to measure"));

  let bs = new BackupService({ FakeBackupResource1, FakeBackupResource2 });
  await bs.takeMeasurements();

  for (let backupResourceClass of [FakeBackupResource1, FakeBackupResource2]) {
    Assert.ok(
      backupResourceClass.prototype.measure.calledOnce,
      "Measure was called"
    );
    Assert.ok(
      backupResourceClass.prototype.measure.calledWith(PathUtils.profileDir),
      "Measure was called with the profile directory argument"
    );
  }

  sandbox.restore();
});

/**
 * Tests that we can measure the disk space available in the profile directory.
 */
add_task(async function test_profDDiskSpace() {
  let bs = new BackupService();
  await bs.takeMeasurements();
  let measurement = Glean.browserBackup.profDDiskSpace.testGetValue();
  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("parent", false, true),
    "browser.backup.prof_d_disk_space",
    measurement
  );

  Assert.greater(
    measurement,
    0,
    "Should have collected a measurement for the profile directory storage " +
      "device"
  );
});

/**
 * Tests that we can measure the Cookies db in a profile directory.
 */
add_task(async function test_cookiesBackupResource() {
  const EXPECTED_COOKIES_DB_SIZE = 1230;

  Services.fog.testResetFOG();

  // Create resource files in temporary directory
  let tempDir = PathUtils.tempDir;
  let tempCookiesDBPath = PathUtils.join(tempDir, "cookies.sqlite");
  await createKilobyteSizedFile(tempCookiesDBPath, EXPECTED_COOKIES_DB_SIZE);

  let cookiesBackupResource = new CookiesBackupResource();
  await cookiesBackupResource.measure(tempDir);

  let cookiesMeasurement = Glean.browserBackup.cookiesSize.testGetValue();
  let scalars = TelemetryTestUtils.getProcessScalars("parent", false, false);

  // Compare glean vs telemetry measurements
  TelemetryTestUtils.assertScalar(
    scalars,
    "browser.backup.cookies_size",
    cookiesMeasurement,
    "Glean and telemetry measurements for cookies.sqlite should be equal"
  );

  // Compare glean measurements vs actual file sizes
  Assert.equal(
    cookiesMeasurement,
    EXPECTED_COOKIES_DB_SIZE,
    "Should have collected the correct glean measurement for cookies.sqlite"
  );

  await maybeRemovePath(tempCookiesDBPath);
});

/**
 * Tests that we can measure the Form History db in a profile directory.
 */
add_task(async function test_formHistoryBackupResource() {
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
 * Tests that we can measure the Session Store JSON and backups directory.
 */
add_task(async function test_sessionStoreBackupResource() {
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
