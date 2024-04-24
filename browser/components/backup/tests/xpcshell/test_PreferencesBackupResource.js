/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PreferencesBackupResource } = ChromeUtils.importESModule(
  "resource:///modules/backup/PreferencesBackupResource.sys.mjs"
);

/**
 * Test that the measure method correctly collects the disk-sizes of things that
 * the PreferencesBackupResource is meant to back up.
 */
add_task(async function test_measure() {
  Services.fog.testResetFOG();

  const EXPECTED_PREFERENCES_KILOBYTES_SIZE = 415;
  const tempDir = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "PreferencesBackupResource-measure-test"
  );
  const mockFiles = [
    { path: "prefs.js", sizeInKB: 20 },
    { path: "xulstore.json", sizeInKB: 1 },
    { path: "permissions.sqlite", sizeInKB: 100 },
    { path: "content-prefs.sqlite", sizeInKB: 260 },
    { path: "containers.json", sizeInKB: 1 },
    { path: "handlers.json", sizeInKB: 1 },
    { path: "search.json.mozlz4", sizeInKB: 1 },
    { path: "user.js", sizeInKB: 2 },
    { path: ["chrome", "userChrome.css"], sizeInKB: 5 },
    { path: ["chrome", "userContent.css"], sizeInKB: 5 },
    { path: ["chrome", "css", "mockStyles.css"], sizeInKB: 5 },
  ];

  await createTestFiles(tempDir, mockFiles);

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

  await maybeRemovePath(tempDir);
});

/**
 * Test that the backup method correctly copies items from the profile directory
 * into the staging directory.
 */
add_task(async function test_backup() {
  let sandbox = sinon.createSandbox();

  let preferencesBackupResource = new PreferencesBackupResource();
  let sourcePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "PreferencesBackupResource-source-test"
  );
  let stagingPath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "PreferencesBackupResource-staging-test"
  );

  const simpleCopyFiles = [
    { path: "xulstore.json" },
    { path: "containers.json" },
    { path: "handlers.json" },
    { path: "search.json.mozlz4" },
    { path: "user.js" },
    { path: ["chrome", "userChrome.css"] },
    { path: ["chrome", "userContent.css"] },
    { path: ["chrome", "childFolder", "someOtherStylesheet.css"] },
  ];
  await createTestFiles(sourcePath, simpleCopyFiles);

  // Create our fake database files. We don't expect these to be copied to the
  // staging directory in this test due to our stubbing of the backup method, so
  // we don't include it in `simpleCopyFiles`.
  await createTestFiles(sourcePath, [
    { path: "permissions.sqlite" },
    { path: "content-prefs.sqlite" },
  ]);

  // We have no need to test that Sqlite.sys.mjs's backup method is working -
  // this is something that is tested in Sqlite's own tests. We can just make
  // sure that it's being called using sinon. Unfortunately, we cannot do the
  // same thing with IOUtils.copy, as its methods are not stubbable.
  let fakeConnection = {
    backup: sandbox.stub().resolves(true),
    close: sandbox.stub().resolves(true),
  };
  sandbox.stub(Sqlite, "openConnection").returns(fakeConnection);

  let manifestEntry = await preferencesBackupResource.backup(
    stagingPath,
    sourcePath
  );
  Assert.equal(
    manifestEntry,
    null,
    "PreferencesBackupResource.backup should return null as its ManifestEntry"
  );

  await assertFilesExist(stagingPath, simpleCopyFiles);

  // Next, we'll make sure that the Sqlite connection had `backup` called on it
  // with the right arguments.
  Assert.ok(
    fakeConnection.backup.calledTwice,
    "Called backup the expected number of times for all connections"
  );
  Assert.ok(
    fakeConnection.backup.firstCall.calledWith(
      PathUtils.join(stagingPath, "permissions.sqlite")
    ),
    "Called backup on the permissions.sqlite Sqlite connection"
  );
  Assert.ok(
    fakeConnection.backup.secondCall.calledWith(
      PathUtils.join(stagingPath, "content-prefs.sqlite")
    ),
    "Called backup on the content-prefs.sqlite Sqlite connection"
  );

  // And we'll make sure that preferences were properly written out.
  Assert.ok(
    await IOUtils.exists(PathUtils.join(stagingPath, "prefs.js")),
    "prefs.js should exist in the staging folder"
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
  let preferencesBackupResource = new PreferencesBackupResource();
  let recoveryPath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "PreferencesBackupResource-recovery-test"
  );
  let destProfilePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "PreferencesBackupResource-test-profile"
  );

  const simpleCopyFiles = [
    { path: "prefs.js" },
    { path: "xulstore.json" },
    { path: "permissions.sqlite" },
    { path: "content-prefs.sqlite" },
    { path: "containers.json" },
    { path: "handlers.json" },
    { path: "search.json.mozlz4" },
    { path: "user.js" },
    { path: ["chrome", "userChrome.css"] },
    { path: ["chrome", "userContent.css"] },
    { path: ["chrome", "childFolder", "someOtherStylesheet.css"] },
  ];
  await createTestFiles(recoveryPath, simpleCopyFiles);

  // The backup method is expected to have returned a null ManifestEntry
  let postRecoveryEntry = await preferencesBackupResource.recover(
    null /* manifestEntry */,
    recoveryPath,
    destProfilePath
  );
  Assert.equal(
    postRecoveryEntry,
    null,
    "PreferencesBackupResource.recover should return null as its post recovery entry"
  );

  await assertFilesExist(destProfilePath, simpleCopyFiles);

  await maybeRemovePath(recoveryPath);
  await maybeRemovePath(destProfilePath);
});
