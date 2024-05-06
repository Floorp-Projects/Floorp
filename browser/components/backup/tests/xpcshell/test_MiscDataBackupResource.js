/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { MiscDataBackupResource } = ChromeUtils.importESModule(
  "resource:///modules/backup/MiscDataBackupResource.sys.mjs"
);

const { ActivityStreamStorage } = ChromeUtils.importESModule(
  "resource://activity-stream/lib/ActivityStreamStorage.sys.mjs"
);

const { ProfileAge } = ChromeUtils.importESModule(
  "resource://gre/modules/ProfileAge.sys.mjs"
);

/**
 * Tests that we can measure miscellaneous files in the profile directory.
 */
add_task(async function test_measure() {
  Services.fog.testResetFOG();

  const EXPECTED_MISC_KILOBYTES_SIZE = 231;
  const tempDir = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "MiscDataBackupResource-measurement-test"
  );

  const mockFiles = [
    { path: "enumerate_devices.txt", sizeInKB: 1 },
    { path: "protections.sqlite", sizeInKB: 100 },
    { path: "SiteSecurityServiceState.bin", sizeInKB: 10 },
    { path: ["storage", "permanent", "chrome", "123ABC.sqlite"], sizeInKB: 40 },
    { path: ["storage", "permanent", "chrome", "456DEF.sqlite"], sizeInKB: 40 },
    {
      path: ["storage", "permanent", "chrome", "mockIDBDir", "890HIJ.sqlite"],
      sizeInKB: 40,
    },
  ];

  await createTestFiles(tempDir, mockFiles);

  let miscDataBackupResource = new MiscDataBackupResource();
  await miscDataBackupResource.measure(tempDir);

  let measurement = Glean.browserBackup.miscDataSize.testGetValue();
  let scalars = TelemetryTestUtils.getProcessScalars("parent", false, false);

  TelemetryTestUtils.assertScalar(
    scalars,
    "browser.backup.misc_data_size",
    measurement,
    "Glean and telemetry measurements for misc data should be equal"
  );
  Assert.equal(
    measurement,
    EXPECTED_MISC_KILOBYTES_SIZE,
    "Should have collected the correct glean measurement for misc files"
  );

  await maybeRemovePath(tempDir);
});

add_task(async function test_backup() {
  let sandbox = sinon.createSandbox();

  let miscDataBackupResource = new MiscDataBackupResource();
  let sourcePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "MiscDataBackupResource-source-test"
  );
  let stagingPath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "MiscDataBackupResource-staging-test"
  );

  const simpleCopyFiles = [
    { path: "enumerate_devices.txt" },
    { path: "SiteSecurityServiceState.bin" },
  ];
  await createTestFiles(sourcePath, simpleCopyFiles);

  // Create our fake database files. We don't expect this to be copied to the
  // staging directory in this test due to our stubbing of the backup method, so
  // we don't include it in `simpleCopyFiles`.
  await createTestFiles(sourcePath, [{ path: "protections.sqlite" }]);

  // We have no need to test that Sqlite.sys.mjs's backup method is working -
  // this is something that is tested in Sqlite's own tests. We can just make
  // sure that it's being called using sinon. Unfortunately, we cannot do the
  // same thing with IOUtils.copy, as its methods are not stubbable.
  let fakeConnection = {
    backup: sandbox.stub().resolves(true),
    close: sandbox.stub().resolves(true),
  };
  sandbox.stub(Sqlite, "openConnection").returns(fakeConnection);

  let snippetsTableStub = {
    getAllKeys: sandbox.stub().resolves(["key1", "key2"]),
    get: sandbox.stub().callsFake(key => {
      return { key: `value for ${key}` };
    }),
  };

  sandbox
    .stub(ActivityStreamStorage.prototype, "getDbTable")
    .withArgs("snippets")
    .resolves(snippetsTableStub);

  let manifestEntry = await miscDataBackupResource.backup(
    stagingPath,
    sourcePath
  );
  Assert.equal(
    manifestEntry,
    null,
    "MiscDataBackupResource.backup should return null as its ManifestEntry"
  );

  await assertFilesExist(stagingPath, simpleCopyFiles);

  // Next, we'll make sure that the Sqlite connection had `backup` called on it
  // with the right arguments.
  Assert.ok(
    fakeConnection.backup.calledOnce,
    "Called backup the expected number of times for all connections"
  );
  Assert.ok(
    fakeConnection.backup.firstCall.calledWith(
      PathUtils.join(stagingPath, "protections.sqlite")
    ),
    "Called backup on the protections.sqlite Sqlite connection"
  );

  // Bug 1890585 - we don't currently have the generalized ability to copy the
  // chrome-privileged IndexedDB databases under storage/permanent/chrome, but
  // we do support copying individual IndexedDB databases by manually exporting
  // and re-importing their contents.
  let snippetsBackupPath = PathUtils.join(
    stagingPath,
    "activity-stream-snippets.json"
  );
  Assert.ok(
    await IOUtils.exists(snippetsBackupPath),
    "The activity-stream-snippets.json file should exist"
  );
  let snippetsBackupContents = await IOUtils.readJSON(snippetsBackupPath);
  Assert.deepEqual(
    snippetsBackupContents,
    {
      key1: { key: "value for key1" },
      key2: { key: "value for key2" },
    },
    "The contents of the activity-stream-snippets.json file should be as expected"
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
  let miscBackupResource = new MiscDataBackupResource();
  let recoveryPath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "MiscDataBackupResource-recovery-test"
  );
  let destProfilePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "MiscDataBackupResource-test-profile"
  );

  // Write a dummy times.json into the xpcshell test profile directory. We
  // expect it to be copied into the destination profile.
  let originalProfileAge = await ProfileAge(PathUtils.profileDir);
  await originalProfileAge.computeAndPersistCreated();
  Assert.ok(
    await IOUtils.exists(PathUtils.join(PathUtils.profileDir, "times.json"))
  );

  const simpleCopyFiles = [
    { path: "enumerate_devices.txt" },
    { path: "protections.sqlite" },
    { path: "SiteSecurityServiceState.bin" },
  ];
  await createTestFiles(recoveryPath, simpleCopyFiles);

  const SNIPPETS_BACKUP_FILE = "activity-stream-snippets.json";

  // We'll also separately create the activity-stream-snippets.json file, which
  // is not expected to be copied into the profile directory, but is expected
  // to exist in the recovery path.
  await createTestFiles(recoveryPath, [{ path: SNIPPETS_BACKUP_FILE }]);

  // The backup method is expected to have returned a null ManifestEntry
  let postRecoveryEntry = await miscBackupResource.recover(
    null /* manifestEntry */,
    recoveryPath,
    destProfilePath
  );
  Assert.deepEqual(
    postRecoveryEntry,
    {
      snippetsBackupFile: PathUtils.join(recoveryPath, SNIPPETS_BACKUP_FILE),
    },
    "MiscDataBackupResource.recover should return the snippets backup data " +
      "path as its post recovery entry"
  );

  await assertFilesExist(destProfilePath, simpleCopyFiles);

  // The activity-stream-snippets.json path should _not_ have been written to
  // the profile path.
  Assert.ok(
    !(await IOUtils.exists(
      PathUtils.join(destProfilePath, SNIPPETS_BACKUP_FILE)
    )),
    "Snippets backup data should not have gone into the profile directory"
  );

  // The times.json file should have been copied over and a backup recovery
  // time written into it.
  Assert.ok(
    await IOUtils.exists(PathUtils.join(destProfilePath, "times.json"))
  );
  let copiedProfileAge = await ProfileAge(destProfilePath);
  Assert.equal(
    await originalProfileAge.created,
    await copiedProfileAge.created,
    "Created timestamp should match."
  );
  Assert.equal(
    await originalProfileAge.firstUse,
    await copiedProfileAge.firstUse,
    "First use timestamp should match."
  );
  Assert.ok(
    await copiedProfileAge.recoveredFromBackup,
    "Backup recovery timestamp should have been set."
  );

  await maybeRemovePath(recoveryPath);
  await maybeRemovePath(destProfilePath);
});

/**
 * Test that the postRecovery method correctly writes the snippets backup data
 * into the snippets IndexedDB table.
 */
add_task(async function test_postRecovery() {
  let sandbox = sinon.createSandbox();

  let fakeProfilePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "MiscDataBackupResource-test-profile"
  );
  let fakeSnippetsData = {
    key1: "value1",
    key2: "value2",
  };
  const SNIPPEST_BACKUP_FILE = PathUtils.join(
    fakeProfilePath,
    "activity-stream-snippets.json"
  );

  await IOUtils.writeJSON(SNIPPEST_BACKUP_FILE, fakeSnippetsData);

  let snippetsTableStub = {
    set: sandbox.stub(),
  };

  sandbox
    .stub(ActivityStreamStorage.prototype, "getDbTable")
    .withArgs("snippets")
    .resolves(snippetsTableStub);

  let miscBackupResource = new MiscDataBackupResource();
  await miscBackupResource.postRecovery({
    snippetsBackupFile: SNIPPEST_BACKUP_FILE,
  });

  Assert.ok(
    snippetsTableStub.set.calledTwice,
    "The snippets table's set method was called twice"
  );
  Assert.ok(
    snippetsTableStub.set.firstCall.calledWith("key1", "value1"),
    "The snippets table's set method was called with the first key-value pair"
  );
  Assert.ok(
    snippetsTableStub.set.secondCall.calledWith("key2", "value2"),
    "The snippets table's set method was called with the second key-value pair"
  );

  sandbox.restore();
});
