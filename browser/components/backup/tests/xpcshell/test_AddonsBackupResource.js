/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AddonsBackupResource } = ChromeUtils.importESModule(
  "resource:///modules/backup/AddonsBackupResource.sys.mjs"
);

/**
 * Tests that we can measure the size of all the addons & extensions data.
 */
add_task(async function test_measure() {
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
  let extensionsXPIAPath = PathUtils.join(
    tempDir,
    "extensions",
    "extension-b.xpi"
  );
  let extensionsXPIBPath = PathUtils.join(
    tempDir,
    "extensions",
    "extension-a.xpi"
  );
  await createKilobyteSizedFile(
    extensionsXPIAPath,
    EXPECTED_KILOBYTES_FOR_EXTENSIONS_XPI_A
  );
  await createKilobyteSizedFile(
    extensionsXPIBPath,
    EXPECTED_KILOBYTES_FOR_EXTENSIONS_XPI_B
  );
  // Should be ignored.
  let extensionsXPIStagedPath = PathUtils.join(
    tempDir,
    "extensions",
    "staged",
    "staged-test-extension.xpi"
  );
  let extensionsXPITrashPath = PathUtils.join(
    tempDir,
    "extensions",
    "trash",
    "trashed-test-extension.xpi"
  );
  let extensionsXPIUnpackedPath = PathUtils.join(
    tempDir,
    "extensions",
    "unpacked-extension.xpi",
    "manifest.json"
  );
  await createKilobyteSizedFile(
    extensionsXPIStagedPath,
    EXPECTED_KILOBYTES_FOR_EXTENSIONS_XPI_C
  );
  await createKilobyteSizedFile(
    extensionsXPITrashPath,
    EXPECTED_KILOBYTES_FOR_EXTENSIONS_XPI_C
  );
  await createKilobyteSizedFile(
    extensionsXPIUnpackedPath,
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

  let extensionsXPIDirectorySizeMeasurement =
    Glean.browserBackup.extensionsXpiDirectorySize.testGetValue();
  Assert.equal(
    extensionsXPIDirectorySizeMeasurement,
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
    extensionsXPIDirectorySizeMeasurement,
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
 * Tests that we can handle the extension store permissions data
 * and moz-extension IndexedDB databases not existing.
 */
add_task(async function test_measure_missing_data() {
  Services.fog.testResetFOG();

  let tempDir = PathUtils.tempDir;

  let extensionsBackupResource = new AddonsBackupResource();
  await extensionsBackupResource.measure(tempDir);

  let extensionStorePermissionsDataSizeMeasurement =
    Glean.browserBackup.extensionStorePermissionsDataSize.testGetValue();
  Assert.equal(
    extensionStorePermissionsDataSizeMeasurement,
    null,
    "Should NOT have collected a measurement for the missing permissions data"
  );

  let extensionsStorageSizeMeasurement =
    Glean.browserBackup.extensionsStorageSize.testGetValue();
  Assert.equal(
    extensionsStorageSizeMeasurement,
    null,
    "Should NOT have collected a measurement for the missing storage data"
  );
});

/**
 * Test that the backup method correctly copies items from the profile directory
 * into the staging directory.
 */
add_task(async function test_backup() {
  let sandbox = sinon.createSandbox();

  let addonsBackupResource = new AddonsBackupResource();
  let sourcePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "AddonsBackupResource-source-test"
  );
  let stagingPath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "AddonsBackupResource-staging-test"
  );

  const simpleCopyFiles = [
    { path: "extensions.json" },
    { path: "extension-settings.json" },
    { path: "extension-preferences.json" },
    { path: "addonStartup.json.lz4" },
    {
      path: [
        "browser-extension-data",
        "{11aa1234-f111-1234-abcd-a9b8c7654d32}",
      ],
    },
    { path: ["extension-store-permissions", "data.safe.bin"] },
    { path: ["extensions", "{11aa1234-f111-1234-abcd-a9b8c7654d32}.xpi"] },
  ];
  await createTestFiles(sourcePath, simpleCopyFiles);

  const junkFiles = [{ path: ["extensions", "junk"] }];
  await createTestFiles(sourcePath, junkFiles);

  // Create a fake storage-sync-v2 database file. We don't expect this to
  // be copied to the staging directory in this test due to our stubbing
  // of the backup method, so we don't include it in `simpleCopyFiles`.
  await createTestFiles(sourcePath, [{ path: "storage-sync-v2.sqlite" }]);

  let fakeConnection = {
    backup: sandbox.stub().resolves(true),
    close: sandbox.stub().resolves(true),
  };
  sandbox.stub(Sqlite, "openConnection").returns(fakeConnection);

  let manifestEntry = await addonsBackupResource.backup(
    stagingPath,
    sourcePath
  );
  Assert.equal(
    manifestEntry,
    null,
    "AddonsBackupResource.backup should return null as its ManifestEntry"
  );

  await assertFilesExist(stagingPath, simpleCopyFiles);

  let junkFile = PathUtils.join(stagingPath, "extensions", "junk");
  Assert.equal(
    await IOUtils.exists(junkFile),
    false,
    `${junkFile} should not exist in the staging folder`
  );

  // Make sure storage-sync-v2 database is backed up.
  Assert.ok(
    fakeConnection.backup.calledOnce,
    "Called backup the expected number of times for all connections"
  );
  Assert.ok(
    fakeConnection.backup.calledWith(
      PathUtils.join(stagingPath, "storage-sync-v2.sqlite")
    ),
    "Called backup on the storage-sync-v2 Sqlite connection"
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
  let addonsBackupResource = new AddonsBackupResource();
  let recoveryPath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "addonsBackupResource-recovery-test"
  );
  let destProfilePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "addonsBackupResource-test-profile"
  );

  const files = [
    { path: "extensions.json" },
    { path: "extension-settings.json" },
    { path: "extension-preferences.json" },
    { path: "addonStartup.json.lz4" },
    { path: "storage-sync-v2.sqlite" },
    { path: ["browser-extension-data", "addon@darkreader.org.xpi", "data"] },
    { path: ["extensions", "addon@darkreader.org.xpi"] },
    { path: ["extension-store-permissions", "data.safe.bin"] },
  ];
  await createTestFiles(recoveryPath, files);

  // The backup method is expected to have returned a null ManifestEntry
  let postRecoveryEntry = await addonsBackupResource.recover(
    null /* manifestEntry */,
    recoveryPath,
    destProfilePath
  );
  Assert.equal(
    postRecoveryEntry,
    null,
    "AddonsBackupResource.recover should return null as its post " +
      "recovery entry"
  );

  await assertFilesExist(destProfilePath, files);

  await maybeRemovePath(recoveryPath);
  await maybeRemovePath(destProfilePath);
});
