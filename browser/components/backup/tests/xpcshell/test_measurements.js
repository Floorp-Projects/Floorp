/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { CredentialsAndSecurityBackupResource } = ChromeUtils.importESModule(
  "resource:///modules/backup/CredentialsAndSecurityBackupResource.sys.mjs"
);
const { PlacesBackupResource } = ChromeUtils.importESModule(
  "resource:///modules/backup/PlacesBackupResource.sys.mjs"
);
const { AddonsBackupResource } = ChromeUtils.importESModule(
  "resource:///modules/backup/AddonsBackupResource.sys.mjs"
);
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

  const EXPECTED_CREDENTIALS_KILOBYTES_SIZE = 413;
  const EXPECTED_SECURITY_KILOBYTES_SIZE = 231;

  // Create resource files in temporary directory
  const tempDir = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "CredentialsAndSecurityBackupResource-measurement-test"
  );

  const mockFiles = [
    // Set up credentials files
    { path: "key4.db", sizeInKB: 300 },
    { path: "logins.json", sizeInKB: 1 },
    { path: "logins-backup.json", sizeInKB: 1 },
    { path: "autofill-profiles.json", sizeInKB: 1 },
    { path: "credentialstate.sqlite", sizeInKB: 100 },
    { path: "signedInUser.json", sizeInKB: 5 },
    // Set up security files
    { path: "cert9.db", sizeInKB: 230 },
    { path: "pkcs11.txt", sizeInKB: 1 },
  ];

  await createTestFiles(tempDir, mockFiles);

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
  await maybeRemovePath(tempDir);
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

/**
 * Tests that we can measure the size of all the addons & extensions data.
 */
add_task(async function test_AddonsBackupResource() {
  Services.fog.testResetFOG();
  Services.telemetry.clearScalars();

  const EXPECTED_KILOBYTES_FOR_EXTENSIONS_JSON = 250;
  const EXPECTED_KILOBYTES_FOR_EXTENSIONS_STORE = 500;
  const EXPECTED_KILOBYTES_FOR_STORAGE_SYNC = 50;
  const EXPECTED_KILOBYTES_FOR_EXTENSIONS_XPI_A = 600;
  const EXPECTED_KILOBYTES_FOR_EXTENSIONS_XPI_B = 400;
  const EXPECTED_KILOBYTES_FOR_EXTENSIONS_XPI_C = 150;
  const EXPECTED_KILOBYTES_FOR_EXTENSIONS_DIRECTORY = 1000;
  const EXPECTED_KILOBYTES_FOR_EXTENSION_DATA = 100;
  const EXPECTED_KILOBYTES_FOR_EXTENSIONS_STORAGE = 200;

  let tempDir = PathUtils.tempDir;

  // Create extensions json files (all the same size).
  const extensionsFilePath = PathUtils.join(tempDir, "extensions.json");
  await createKilobyteSizedFile(
    extensionsFilePath,
    EXPECTED_KILOBYTES_FOR_EXTENSIONS_JSON
  );
  const extensionSettingsFilePath = PathUtils.join(
    tempDir,
    "extension-settings.json"
  );
  await createKilobyteSizedFile(
    extensionSettingsFilePath,
    EXPECTED_KILOBYTES_FOR_EXTENSIONS_JSON
  );
  const extensionsPrefsFilePath = PathUtils.join(
    tempDir,
    "extension-preferences.json"
  );
  await createKilobyteSizedFile(
    extensionsPrefsFilePath,
    EXPECTED_KILOBYTES_FOR_EXTENSIONS_JSON
  );
  const addonStartupFilePath = PathUtils.join(tempDir, "addonStartup.json.lz4");
  await createKilobyteSizedFile(
    addonStartupFilePath,
    EXPECTED_KILOBYTES_FOR_EXTENSIONS_JSON
  );

  // Create the extension store permissions data file.
  let extensionStorePermissionsDataSize = PathUtils.join(
    tempDir,
    "extension-store-permissions",
    "data.safe.bin"
  );
  await createKilobyteSizedFile(
    extensionStorePermissionsDataSize,
    EXPECTED_KILOBYTES_FOR_EXTENSIONS_STORE
  );

  // Create the storage sync database file.
  let storageSyncPath = PathUtils.join(tempDir, "storage-sync-v2.sqlite");
  await createKilobyteSizedFile(
    storageSyncPath,
    EXPECTED_KILOBYTES_FOR_STORAGE_SYNC
  );

  // Create the extensions directory with XPI files.
  let extensionsXpiAPath = PathUtils.join(
    tempDir,
    "extensions",
    "extension-b.xpi"
  );
  let extensionsXpiBPath = PathUtils.join(
    tempDir,
    "extensions",
    "extension-a.xpi"
  );
  await createKilobyteSizedFile(
    extensionsXpiAPath,
    EXPECTED_KILOBYTES_FOR_EXTENSIONS_XPI_A
  );
  await createKilobyteSizedFile(
    extensionsXpiBPath,
    EXPECTED_KILOBYTES_FOR_EXTENSIONS_XPI_B
  );
  // Should be ignored.
  let extensionsXpiStagedPath = PathUtils.join(
    tempDir,
    "extensions",
    "staged",
    "staged-test-extension.xpi"
  );
  let extensionsXpiTrashPath = PathUtils.join(
    tempDir,
    "extensions",
    "trash",
    "trashed-test-extension.xpi"
  );
  let extensionsXpiUnpackedPath = PathUtils.join(
    tempDir,
    "extensions",
    "unpacked-extension.xpi",
    "manifest.json"
  );
  await createKilobyteSizedFile(
    extensionsXpiStagedPath,
    EXPECTED_KILOBYTES_FOR_EXTENSIONS_XPI_C
  );
  await createKilobyteSizedFile(
    extensionsXpiTrashPath,
    EXPECTED_KILOBYTES_FOR_EXTENSIONS_XPI_C
  );
  await createKilobyteSizedFile(
    extensionsXpiUnpackedPath,
    EXPECTED_KILOBYTES_FOR_EXTENSIONS_XPI_C
  );

  // Create the browser extension data directory.
  let browserExtensionDataPath = PathUtils.join(
    tempDir,
    "browser-extension-data",
    "test-file"
  );
  await createKilobyteSizedFile(
    browserExtensionDataPath,
    EXPECTED_KILOBYTES_FOR_EXTENSION_DATA
  );

  // Create the extensions storage directory.
  let extensionsStoragePath = PathUtils.join(
    tempDir,
    "storage",
    "default",
    "moz-extension+++test-extension-id",
    "idb",
    "data.sqlite"
  );
  // Other storage files that should not be counted.
  let otherStoragePath = PathUtils.join(
    tempDir,
    "storage",
    "default",
    "https+++accounts.firefox.com",
    "ls",
    "data.sqlite"
  );

  await createKilobyteSizedFile(
    extensionsStoragePath,
    EXPECTED_KILOBYTES_FOR_EXTENSIONS_STORAGE
  );
  await createKilobyteSizedFile(
    otherStoragePath,
    EXPECTED_KILOBYTES_FOR_EXTENSIONS_STORAGE
  );

  // Measure all the extensions data.
  let extensionsBackupResource = new AddonsBackupResource();
  await extensionsBackupResource.measure(tempDir);

  let extensionsJsonSizeMeasurement =
    Glean.browserBackup.extensionsJsonSize.testGetValue();
  Assert.equal(
    extensionsJsonSizeMeasurement,
    EXPECTED_KILOBYTES_FOR_EXTENSIONS_JSON * 4, // There are 4 equally sized files.
    "Should have collected the correct measurement of the total size of all extensions JSON files"
  );

  let extensionStorePermissionsDataSizeMeasurement =
    Glean.browserBackup.extensionStorePermissionsDataSize.testGetValue();
  Assert.equal(
    extensionStorePermissionsDataSizeMeasurement,
    EXPECTED_KILOBYTES_FOR_EXTENSIONS_STORE,
    "Should have collected the correct measurement of the size of the extension store permissions data"
  );

  let storageSyncSizeMeasurement =
    Glean.browserBackup.storageSyncSize.testGetValue();
  Assert.equal(
    storageSyncSizeMeasurement,
    EXPECTED_KILOBYTES_FOR_STORAGE_SYNC,
    "Should have collected the correct measurement of the size of the storage sync database"
  );

  let extensionsXpiDirectorySizeMeasurement =
    Glean.browserBackup.extensionsXpiDirectorySize.testGetValue();
  Assert.equal(
    extensionsXpiDirectorySizeMeasurement,
    EXPECTED_KILOBYTES_FOR_EXTENSIONS_DIRECTORY,
    "Should have collected the correct measurement of the size 2 equally sized XPI files in the extensions directory"
  );

  let browserExtensionDataSizeMeasurement =
    Glean.browserBackup.browserExtensionDataSize.testGetValue();
  Assert.equal(
    browserExtensionDataSizeMeasurement,
    EXPECTED_KILOBYTES_FOR_EXTENSION_DATA,
    "Should have collected the correct measurement of the size of the browser extension data directory"
  );

  let extensionsStorageSizeMeasurement =
    Glean.browserBackup.extensionsStorageSize.testGetValue();
  Assert.equal(
    extensionsStorageSizeMeasurement,
    EXPECTED_KILOBYTES_FOR_EXTENSIONS_STORAGE,
    "Should have collected the correct measurement of all the extensions storage"
  );

  // Compare glean vs telemetry measurements
  let scalars = TelemetryTestUtils.getProcessScalars("parent", false, false);
  TelemetryTestUtils.assertScalar(
    scalars,
    "browser.backup.extensions_json_size",
    extensionsJsonSizeMeasurement,
    "Glean and telemetry measurements for extensions JSON should be equal"
  );
  TelemetryTestUtils.assertScalar(
    scalars,
    "browser.backup.extension_store_permissions_data_size",
    extensionStorePermissionsDataSizeMeasurement,
    "Glean and telemetry measurements for extension store permissions data should be equal"
  );
  TelemetryTestUtils.assertScalar(
    scalars,
    "browser.backup.storage_sync_size",
    storageSyncSizeMeasurement,
    "Glean and telemetry measurements for storage sync database should be equal"
  );
  TelemetryTestUtils.assertScalar(
    scalars,
    "browser.backup.extensions_xpi_directory_size",
    extensionsXpiDirectorySizeMeasurement,
    "Glean and telemetry measurements for extensions directory should be equal"
  );
  TelemetryTestUtils.assertScalar(
    scalars,
    "browser.backup.browser_extension_data_size",
    browserExtensionDataSizeMeasurement,
    "Glean and telemetry measurements for browser extension data should be equal"
  );
  TelemetryTestUtils.assertScalar(
    scalars,
    "browser.backup.extensions_storage_size",
    extensionsStorageSizeMeasurement,
    "Glean and telemetry measurements for extensions storage should be equal"
  );

  await maybeRemovePath(tempDir);
});

/**
 * Tests that we can handle the extension store permissions data not existing.
 */
add_task(
  async function test_AddonsBackupResource_no_extension_store_permissions_data() {
    Services.fog.testResetFOG();

    let tempDir = PathUtils.tempDir;

    let extensionsBackupResource = new AddonsBackupResource();
    await extensionsBackupResource.measure(tempDir);

    let extensionStorePermissionsDataSizeMeasurement =
      Glean.browserBackup.extensionStorePermissionsDataSize.testGetValue();
    Assert.equal(
      extensionStorePermissionsDataSizeMeasurement,
      null,
      "Should NOT have collected a measurement for the missing data"
    );
  }
);

/**
 * Tests that we can handle a profile with no moz-extension IndexedDB databases.
 */
add_task(
  async function test_AddonsBackupResource_no_extension_storage_databases() {
    Services.fog.testResetFOG();

    let tempDir = PathUtils.tempDir;

    let extensionsBackupResource = new AddonsBackupResource();
    await extensionsBackupResource.measure(tempDir);

    let extensionsStorageSizeMeasurement =
      Glean.browserBackup.extensionsStorageSize.testGetValue();
    Assert.equal(
      extensionsStorageSizeMeasurement,
      null,
      "Should NOT have collected a measurement for the missing data"
    );
  }
);
