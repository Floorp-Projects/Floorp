/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { BackupResource } = ChromeUtils.importESModule(
  "resource:///modules/backup/BackupResource.sys.mjs"
);
const { BackupService } = ChromeUtils.importESModule(
  "resource:///modules/backup/BackupService.sys.mjs"
);
const { CredentialsAndSecurityBackupResource } = ChromeUtils.importESModule(
  "resource:///modules/backup/CredentialsAndSecurityBackupResource.sys.mjs"
);
const { PlacesBackupResource } = ChromeUtils.importESModule(
  "resource:///modules/backup/PlacesBackupResource.sys.mjs"
);
const { PreferencesBackupResource } = ChromeUtils.importESModule(
  "resource:///modules/backup/PreferencesBackupResource.sys.mjs"
);

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

add_setup(() => {
  do_get_profile();
  // FOG needs to be initialized in order for data to flow.
  Services.fog.initializeFOG();
  Services.telemetry.clearScalars();
});

/**
 * Tests that calling `BackupService.takeMeasurements` will call the measure
 * method of all registered BackupResource classes.
 */
add_task(async function test_takeMeasurements() {
  /**
   * Some fake backup resource classes to test with.
   */
  class FakeBackupResource1 extends BackupResource {
    static get key() {
      return "fake1";
    }
    measure() {}
  }

  /**
   * Another fake backup resource class to test with.
   */
  class FakeBackupResource2 extends BackupResource {
    static get key() {
      return "fake2";
    }
    measure() {}
  }

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
 * Tests that we can measure Places DB related files in the profile directory.
 */
add_task(async function test_placesBackupResource() {
  Services.fog.testResetFOG();

  const EXPECTED_PLACES_DB_SIZE = 5240;
  const EXPECTED_FAVICONS_DB_SIZE = 5240;

  // Create resource files in temporary directory
  const tempDir = PathUtils.tempDir;
  let tempPlacesDBPath = PathUtils.join(tempDir, "places.sqlite");
  let tempFaviconsDBPath = PathUtils.join(tempDir, "favicons.sqlite");
  await createKilobyteSizedFile(tempPlacesDBPath, EXPECTED_PLACES_DB_SIZE);
  await createKilobyteSizedFile(tempFaviconsDBPath, EXPECTED_FAVICONS_DB_SIZE);

  let placesBackupResource = new PlacesBackupResource();
  await placesBackupResource.measure(tempDir);

  let placesMeasurement = Glean.browserBackup.placesSize.testGetValue();
  let faviconsMeasurement = Glean.browserBackup.faviconsSize.testGetValue();
  let scalars = TelemetryTestUtils.getProcessScalars("parent", false, false);

  // Compare glean vs telemetry measurements
  TelemetryTestUtils.assertScalar(
    scalars,
    "browser.backup.places_size",
    placesMeasurement,
    "Glean and telemetry measurements for places.sqlite should be equal"
  );
  TelemetryTestUtils.assertScalar(
    scalars,
    "browser.backup.favicons_size",
    faviconsMeasurement,
    "Glean and telemetry measurements for favicons.sqlite should be equal"
  );

  // Compare glean measurements vs actual file sizes
  Assert.equal(
    placesMeasurement,
    EXPECTED_PLACES_DB_SIZE,
    "Should have collected the correct glean measurement for places.sqlite"
  );
  Assert.equal(
    faviconsMeasurement,
    EXPECTED_FAVICONS_DB_SIZE,
    "Should have collected the correct glean measurement for favicons.sqlite"
  );

  await IOUtils.remove(tempPlacesDBPath);
  await IOUtils.remove(tempFaviconsDBPath);
});

/**
 * Tests that we can measure credentials related files in the profile directory.
 */
add_task(async function test_credentialsAndSecurityBackupResource() {
  Services.fog.testResetFOG();

  const EXPECTED_CREDENTIALS_KILOBYTES_SIZE = 403;
  const EXPECTED_SECURITY_KILOBYTES_SIZE = 231;

  // Create resource files in temporary directory
  const tempDir = PathUtils.tempDir;

  // Set up credentials files
  const mockCredentialsFiles = new Map([
    ["key4.db", 300],
    ["logins.json", 1],
    ["logins-backup.json", 1],
    ["autofill-profiles.json", 1],
    ["credentialstate.sqlite", 100],
  ]);

  for (let [mockFileName, mockFileSize] of mockCredentialsFiles) {
    let tempPath = PathUtils.join(tempDir, mockFileName);
    await createKilobyteSizedFile(tempPath, mockFileSize);
  }

  // Set up security files
  const mockSecurityFiles = new Map([
    ["cert9.db", 230],
    ["pkcs11.txt", 1],
  ]);

  for (let [mockFileName, mockFileSize] of mockSecurityFiles) {
    let tempPath = PathUtils.join(tempDir, mockFileName);
    await createKilobyteSizedFile(tempPath, mockFileSize);
  }

  let credentialsAndSecurityBackupResource =
    new CredentialsAndSecurityBackupResource();
  await credentialsAndSecurityBackupResource.measure(tempDir);

  let credentialsMeasurement =
    Glean.browserBackup.credentialsDataSize.testGetValue();
  let securityMeasurement = Glean.browserBackup.securityDataSize.testGetValue();
  let scalars = TelemetryTestUtils.getProcessScalars("parent", false, false);

  // Credentials measurements
  TelemetryTestUtils.assertScalar(
    scalars,
    "browser.backup.credentials_data_size",
    credentialsMeasurement,
    "Glean and telemetry measurements for credentials data should be equal"
  );

  Assert.equal(
    credentialsMeasurement,
    EXPECTED_CREDENTIALS_KILOBYTES_SIZE,
    "Should have collected the correct glean measurement for credentials files"
  );

  // Security measurements
  TelemetryTestUtils.assertScalar(
    scalars,
    "browser.backup.security_data_size",
    securityMeasurement,
    "Glean and telemetry measurements for security data should be equal"
  );
  Assert.equal(
    securityMeasurement,
    EXPECTED_SECURITY_KILOBYTES_SIZE,
    "Should have collected the correct glean measurement for security files"
  );

  // Cleanup
  for (let mockFileName of mockCredentialsFiles.keys()) {
    let tempPath = PathUtils.join(tempDir, mockFileName);
    await IOUtils.remove(tempPath);
  }
  for (let mockFileName of mockSecurityFiles.keys()) {
    let tempPath = PathUtils.join(tempDir, mockFileName);
    await IOUtils.remove(tempPath);
  }
});

add_task(async function test_preferencesBackupResource() {
  Services.fog.testResetFOG();

  const EXPECTED_PREFERENCES_KILOBYTES_SIZE = 415;
  const tempDir = PathUtils.tempDir;
  const mockFiles = new Map([
    ["prefs.js", 20],
    ["xulstore.json", 1],
    ["permissions.sqlite", 100],
    ["content-prefs.sqlite", 260],
    ["containers.json", 1],
    ["handlers.json", 1],
    ["search.json.mozlz4", 1],
    ["user.js", 2],
  ]);
  const mockChromeDirFiles = new Map([
    ["userChrome.css", 5],
    ["userContent.css", 5],
    ["css/mockStyles.css", 5],
  ]);

  for (let [mockFilePath, mockFileSize] of mockFiles) {
    let tempPath = PathUtils.join(tempDir, mockFilePath);
    await createKilobyteSizedFile(tempPath, mockFileSize);
  }

  for (let [mockFilePath, mockFileSize] of mockChromeDirFiles) {
    // To avoid issues with forward or backward slashes when testing
    // on windows or macOS/linux, split and remake the filepath using `PathUtils.join`.
    let processedMockFileName = mockFilePath.split(/[\\\/]/);
    let tempPath = PathUtils.join(tempDir, "chrome", ...processedMockFileName);
    await createKilobyteSizedFile(tempPath, mockFileSize);
  }

  let preferencesBackupResource = new PreferencesBackupResource();
  await preferencesBackupResource.measure(tempDir);

  let measurement = Glean.browserBackup.preferencesSize.testGetValue();
  let scalars = TelemetryTestUtils.getProcessScalars("parent", false, false);

  TelemetryTestUtils.assertScalar(
    scalars,
    "browser.backup.preferences_size",
    measurement,
    "Glean and telemetry measurements for preferences data should be equal"
  );
  Assert.equal(
    measurement,
    EXPECTED_PREFERENCES_KILOBYTES_SIZE,
    "Should have collected the correct glean measurement for preferences files"
  );

  for (let mockFileName of mockFiles.keys()) {
    let tempPath = PathUtils.join(tempDir, mockFileName);
    await IOUtils.remove(tempPath);
  }

  for (let mockFilePath of mockChromeDirFiles.keys()) {
    let processedMockFilePath = mockFilePath.split(/[\\\/]/);
    let tempPath = PathUtils.join(tempDir, "chrome", processedMockFilePath);
    await IOUtils.remove(tempPath, { recursive: true });
  }
});
