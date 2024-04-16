/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { MiscDataBackupResource } = ChromeUtils.importESModule(
  "resource:///modules/backup/MiscDataBackupResource.sys.mjs"
);

const { ActivityStreamStorage } = ChromeUtils.importESModule(
  "resource://activity-stream/lib/ActivityStreamStorage.sys.mjs"
);

/**
 * Tests that we can measure miscellaneous files in the profile directory.
 */
add_task(async function test_measure() {
  Services.fog.testResetFOG();

  const EXPECTED_MISC_KILOBYTES_SIZE = 241;
  const tempDir = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "MiscDataBackupResource-measurement-test"
  );

  const mockFiles = [
    { path: "times.json", sizeInKB: 5 },
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
    { path: "times.json" },
    { path: "enumerate_devices.txt" },
    { path: "SiteSecurityServiceState.bin" },
  ];
  await createTestFiles(sourcePath, simpleCopyFiles);

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

  await miscDataBackupResource.backup(stagingPath, sourcePath);

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
