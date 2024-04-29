/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { BookmarkJSONUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/BookmarkJSONUtils.sys.mjs"
);
const { PlacesBackupResource } = ChromeUtils.importESModule(
  "resource:///modules/backup/PlacesBackupResource.sys.mjs"
);
const { PrivateBrowsingUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/PrivateBrowsingUtils.sys.mjs"
);

const HISTORY_ENABLED_PREF = "places.history.enabled";
const SANITIZE_ON_SHUTDOWN_PREF = "privacy.sanitize.sanitizeOnShutdown";

registerCleanupFunction(() => {
  /**
   * Even though test_backup_no_saved_history clears user prefs too,
   * clear them here as well in case that test fails and we don't
   * reach the end of the test, which handles the cleanup.
   */
  Services.prefs.clearUserPref(HISTORY_ENABLED_PREF);
  Services.prefs.clearUserPref(SANITIZE_ON_SHUTDOWN_PREF);
});

/**
 * Tests that we can measure Places DB related files in the profile directory.
 */
add_task(async function test_measure() {
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

  await maybeRemovePath(tempPlacesDBPath);
  await maybeRemovePath(tempFaviconsDBPath);
});

/**
 * Tests that the backup method correctly copies places.sqlite and
 * favicons.sqlite from the profile directory into the staging directory.
 */
add_task(async function test_backup() {
  let sandbox = sinon.createSandbox();

  let placesBackupResource = new PlacesBackupResource();
  let sourcePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "PlacesBackupResource-source-test"
  );
  let stagingPath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "PlacesBackupResource-staging-test"
  );

  // Make sure these files exist in the source directory, otherwise
  // BackupResource will skip attempting to back them up.
  await createTestFiles(sourcePath, [
    { path: "places.sqlite" },
    { path: "favicons.sqlite" },
  ]);

  let fakeConnection = {
    backup: sandbox.stub().resolves(true),
    close: sandbox.stub().resolves(true),
  };
  sandbox.stub(Sqlite, "openConnection").returns(fakeConnection);

  let manifestEntry = await placesBackupResource.backup(
    stagingPath,
    sourcePath
  );
  Assert.equal(
    manifestEntry,
    null,
    "PlacesBackupResource.backup should return null as its ManifestEntry"
  );

  Assert.ok(
    fakeConnection.backup.calledTwice,
    "Backup should have been called twice"
  );
  Assert.ok(
    fakeConnection.backup.firstCall.calledWith(
      PathUtils.join(stagingPath, "places.sqlite")
    ),
    "places.sqlite should have been backed up first"
  );
  Assert.ok(
    fakeConnection.backup.secondCall.calledWith(
      PathUtils.join(stagingPath, "favicons.sqlite")
    ),
    "favicons.sqlite should have been backed up second"
  );

  await maybeRemovePath(stagingPath);
  await maybeRemovePath(sourcePath);

  sandbox.restore();
});

/**
 * Tests that the backup method correctly creates a compressed bookmarks JSON file when users
 * don't want history saved, even on shutdown.
 */
add_task(async function test_backup_no_saved_history() {
  let sandbox = sinon.createSandbox();

  let placesBackupResource = new PlacesBackupResource();
  let sourcePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "PlacesBackupResource-source-test"
  );
  let stagingPath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "PlacesBackupResource-staging-test"
  );

  let fakeConnection = {
    backup: sandbox.stub().resolves(true),
    close: sandbox.stub().resolves(true),
  };
  sandbox.stub(Sqlite, "openConnection").returns(fakeConnection);

  /**
   * First verify that remember history pref alone affects backup file type for places,
   * despite sanitize on shutdown pref value.
   */
  Services.prefs.setBoolPref(HISTORY_ENABLED_PREF, false);
  Services.prefs.setBoolPref(SANITIZE_ON_SHUTDOWN_PREF, false);

  let manifestEntry = await placesBackupResource.backup(
    stagingPath,
    sourcePath
  );
  Assert.deepEqual(
    manifestEntry,
    { bookmarksOnly: true },
    "Should have gotten back a ManifestEntry indicating that we only copied " +
      "bookmarks"
  );

  Assert.ok(
    fakeConnection.backup.notCalled,
    "No sqlite connections should have been made with remember history disabled"
  );
  await assertFilesExist(stagingPath, [{ path: "bookmarks.jsonlz4" }]);
  await IOUtils.remove(PathUtils.join(stagingPath, "bookmarks.jsonlz4"));

  /**
   * Now verify that the sanitize shutdown pref alone affects backup file type for places,
   * even if the user is okay with remembering history while browsing.
   */
  Services.prefs.setBoolPref(HISTORY_ENABLED_PREF, true);
  Services.prefs.setBoolPref(SANITIZE_ON_SHUTDOWN_PREF, true);

  fakeConnection.backup.resetHistory();
  manifestEntry = await placesBackupResource.backup(stagingPath, sourcePath);
  Assert.deepEqual(
    manifestEntry,
    { bookmarksOnly: true },
    "Should have gotten back a ManifestEntry indicating that we only copied " +
      "bookmarks"
  );

  Assert.ok(
    fakeConnection.backup.notCalled,
    "No sqlite connections should have been made with sanitize shutdown enabled"
  );
  await assertFilesExist(stagingPath, [{ path: "bookmarks.jsonlz4" }]);

  await maybeRemovePath(stagingPath);
  await maybeRemovePath(sourcePath);

  sandbox.restore();
  Services.prefs.clearUserPref(HISTORY_ENABLED_PREF);
  Services.prefs.clearUserPref(SANITIZE_ON_SHUTDOWN_PREF);
});

/**
 * Tests that the backup method correctly creates a compressed bookmarks JSON file when
 * permanent private browsing mode is enabled.
 */
add_task(async function test_backup_private_browsing() {
  let sandbox = sinon.createSandbox();

  let placesBackupResource = new PlacesBackupResource();
  let sourcePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "PlacesBackupResource-source-test"
  );
  let stagingPath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "PlacesBackupResource-staging-test"
  );

  let fakeConnection = {
    backup: sandbox.stub().resolves(true),
    close: sandbox.stub().resolves(true),
  };
  sandbox.stub(Sqlite, "openConnection").returns(fakeConnection);
  sandbox.stub(PrivateBrowsingUtils, "permanentPrivateBrowsing").value(true);

  let manifestEntry = await placesBackupResource.backup(
    stagingPath,
    sourcePath
  );
  Assert.deepEqual(
    manifestEntry,
    { bookmarksOnly: true },
    "Should have gotten back a ManifestEntry indicating that we only copied " +
      "bookmarks"
  );

  Assert.ok(
    fakeConnection.backup.notCalled,
    "No sqlite connections should have been made with permanent private browsing enabled"
  );
  await assertFilesExist(stagingPath, [{ path: "bookmarks.jsonlz4" }]);

  await maybeRemovePath(stagingPath);
  await maybeRemovePath(sourcePath);

  sandbox.restore();
});

/**
 * Test that the recover method correctly copies places.sqlite and favicons.sqlite
 * from the recovery directory into the destination profile directory.
 */
add_task(async function test_recover() {
  let placesBackupResource = new PlacesBackupResource();
  let recoveryPath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "PlacesBackupResource-recovery-test"
  );
  let destProfilePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "PlacesBackupResource-test-profile"
  );

  const simpleCopyFiles = [
    { path: "places.sqlite" },
    { path: "favicons.sqlite" },
  ];
  await createTestFiles(recoveryPath, simpleCopyFiles);

  // The backup method is expected to have returned a null ManifestEntry
  let postRecoveryEntry = await placesBackupResource.recover(
    null /* manifestEntry */,
    recoveryPath,
    destProfilePath
  );
  Assert.equal(
    postRecoveryEntry,
    null,
    "PlacesBackupResource.recover should return null as its post recovery entry"
  );

  await assertFilesExist(destProfilePath, simpleCopyFiles);

  await maybeRemovePath(recoveryPath);
  await maybeRemovePath(destProfilePath);
});

/**
 * Test that the recover method correctly copies bookmarks.jsonlz4 from the recovery
 * directory into the destination profile directory.
 */
add_task(async function test_recover_bookmarks_only() {
  let sandbox = sinon.createSandbox();
  let placesBackupResource = new PlacesBackupResource();
  let recoveryPath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "PlacesBackupResource-recovery-test"
  );
  let destProfilePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "PlacesBackupResource-test-profile"
  );
  let bookmarksImportStub = sandbox
    .stub(BookmarkJSONUtils, "importFromFile")
    .resolves(true);

  await createTestFiles(recoveryPath, [{ path: "bookmarks.jsonlz4" }]);

  // The backup method is expected to detect bookmarks import only
  let postRecoveryEntry = await placesBackupResource.recover(
    { bookmarksOnly: true },
    recoveryPath,
    destProfilePath
  );

  let expectedBookmarksPath = PathUtils.join(recoveryPath, "bookmarks.jsonlz4");

  // Expect the bookmarks backup file path to be passed from recover()
  Assert.deepEqual(
    postRecoveryEntry,
    { bookmarksBackupPath: expectedBookmarksPath },
    "PlacesBackupResource.recover should return the expected post recovery entry"
  );

  // Ensure that files stored in a places backup are not copied to the new profile during recovery
  for (let placesFile of [
    "places.sqlite",
    "favicons.sqlite",
    "bookmarks.jsonlz4",
  ]) {
    Assert.ok(
      !(await IOUtils.exists(PathUtils.join(destProfilePath, placesFile))),
      `${placesFile} should not exist in the new profile`
    );
  }

  // Now pretend that BackupService called the postRecovery method
  await placesBackupResource.postRecovery(postRecoveryEntry);
  Assert.ok(
    bookmarksImportStub.calledOnce,
    "BookmarkJSONUtils.importFromFile was called in the postRecovery step"
  );

  await maybeRemovePath(recoveryPath);
  await maybeRemovePath(destProfilePath);

  sandbox.restore();
});
