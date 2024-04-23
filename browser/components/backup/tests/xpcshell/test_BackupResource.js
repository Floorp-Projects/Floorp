/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { bytesToFuzzyKilobytes } = ChromeUtils.importESModule(
  "resource:///modules/backup/BackupResource.sys.mjs"
);

const EXPECTED_KILOBYTES_FOR_XULSTORE = 1;

/**
 * Tests that BackupService.getFileSize will get the size of a file in kilobytes.
 */
add_task(async function test_getFileSize() {
  let file = do_get_file("data/test_xulstore.json");

  let testFilePath = PathUtils.join(PathUtils.profileDir, "test_xulstore.json");

  await IOUtils.copy(file.path, PathUtils.profileDir);

  let size = await BackupResource.getFileSize(testFilePath);

  Assert.equal(
    size,
    EXPECTED_KILOBYTES_FOR_XULSTORE,
    "Size of the test_xulstore.json is rounded up to the nearest kilobyte."
  );

  await IOUtils.remove(testFilePath);
});

/**
 * Tests that BackupService.getDirectorySize will get the total size of all the
 * files in a directory and it's children in kilobytes.
 */
add_task(async function test_getDirectorySize() {
  let file = do_get_file("data/test_xulstore.json");

  // Create a test directory with the test json file in it.
  let testDir = PathUtils.join(PathUtils.profileDir, "testDir");
  await IOUtils.makeDirectory(testDir);
  await IOUtils.copy(file.path, testDir);

  // Create another test directory inside of that one.
  let nestedTestDir = PathUtils.join(testDir, "testDir");
  await IOUtils.makeDirectory(nestedTestDir);
  await IOUtils.copy(file.path, nestedTestDir);

  let size = await BackupResource.getDirectorySize(testDir);

  Assert.equal(
    size,
    EXPECTED_KILOBYTES_FOR_XULSTORE * 2,
    `Total size of the directory is rounded up to the nearest kilobyte
    and is equal to twice the size of the test_xulstore.json file`
  );

  await IOUtils.remove(testDir, { recursive: true });
});

/**
 * Tests that bytesToFuzzyKilobytes will convert bytes to kilobytes
 * and round up to the nearest tenth kilobyte.
 */
add_task(async function test_bytesToFuzzyKilobytes() {
  let largeSize = bytesToFuzzyKilobytes(1234000);

  Assert.equal(
    largeSize,
    1230,
    "1234 bytes is rounded up to the nearest tenth kilobyte, 1230"
  );

  let smallSize = bytesToFuzzyKilobytes(3);

  Assert.equal(smallSize, 1, "Sizes under 10 kilobytes return 1 kilobyte");
});

/**
 * Tests that BackupResource.copySqliteDatabases will call `backup` on a new
 * read-only connection on each database file.
 */
add_task(async function test_copySqliteDatabases() {
  let sandbox = sinon.createSandbox();
  const SQLITE_PAGES_PER_STEP_PREF = "browser.backup.sqlite.pages_per_step";
  const SQLITE_STEP_DELAY_MS_PREF = "browser.backup.sqlite.step_delay_ms";
  const DEFAULT_SQLITE_PAGES_PER_STEP = Services.prefs.getIntPref(
    SQLITE_PAGES_PER_STEP_PREF
  );
  const DEFAULT_SQLITE_STEP_DELAY_MS = Services.prefs.getIntPref(
    SQLITE_STEP_DELAY_MS_PREF
  );

  let sourcePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "BackupResource-source-test"
  );
  let destPath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "BackupResource-dest-test"
  );
  let pretendDatabases = ["places.sqlite", "favicons.sqlite"];
  await createTestFiles(
    sourcePath,
    pretendDatabases.map(f => ({ path: f }))
  );

  let fakeConnection = {
    backup: sandbox.stub().resolves(true),
    close: sandbox.stub().resolves(true),
  };
  sandbox.stub(Sqlite, "openConnection").returns(fakeConnection);

  await BackupResource.copySqliteDatabases(
    sourcePath,
    destPath,
    pretendDatabases
  );

  Assert.ok(
    Sqlite.openConnection.calledTwice,
    "Sqlite.openConnection called twice"
  );
  Assert.ok(
    Sqlite.openConnection.firstCall.calledWith({
      path: PathUtils.join(sourcePath, "places.sqlite"),
      readOnly: true,
    }),
    "openConnection called with places.sqlite as read-only"
  );
  Assert.ok(
    Sqlite.openConnection.secondCall.calledWith({
      path: PathUtils.join(sourcePath, "favicons.sqlite"),
      readOnly: true,
    }),
    "openConnection called with favicons.sqlite as read-only"
  );

  Assert.ok(
    fakeConnection.backup.calledTwice,
    "backup on an Sqlite connection called twice"
  );
  Assert.ok(
    fakeConnection.backup.firstCall.calledWith(
      PathUtils.join(destPath, "places.sqlite"),
      DEFAULT_SQLITE_PAGES_PER_STEP,
      DEFAULT_SQLITE_STEP_DELAY_MS
    ),
    "backup called with places.sqlite to the destination path with the right " +
      "pages per step and step delay"
  );
  Assert.ok(
    fakeConnection.backup.secondCall.calledWith(
      PathUtils.join(destPath, "favicons.sqlite"),
      DEFAULT_SQLITE_PAGES_PER_STEP,
      DEFAULT_SQLITE_STEP_DELAY_MS
    ),
    "backup called with favicons.sqlite to the destination path with the " +
      "right pages per step and step delay"
  );

  Assert.ok(
    fakeConnection.close.calledTwice,
    "close on an Sqlite connection called twice"
  );

  // Now check that we can override the default pages per step and step delay.
  fakeConnection.backup.resetHistory();
  const NEW_SQLITE_PAGES_PER_STEP = 10;
  const NEW_SQLITE_STEP_DELAY_MS = 500;
  Services.prefs.setIntPref(
    SQLITE_PAGES_PER_STEP_PREF,
    NEW_SQLITE_PAGES_PER_STEP
  );
  Services.prefs.setIntPref(
    SQLITE_STEP_DELAY_MS_PREF,
    NEW_SQLITE_STEP_DELAY_MS
  );
  await BackupResource.copySqliteDatabases(
    sourcePath,
    destPath,
    pretendDatabases
  );
  Assert.ok(
    fakeConnection.backup.calledTwice,
    "backup on an Sqlite connection called twice"
  );
  Assert.ok(
    fakeConnection.backup.firstCall.calledWith(
      PathUtils.join(destPath, "places.sqlite"),
      NEW_SQLITE_PAGES_PER_STEP,
      NEW_SQLITE_STEP_DELAY_MS
    ),
    "backup called with places.sqlite to the destination path with the right " +
      "pages per step and step delay"
  );
  Assert.ok(
    fakeConnection.backup.secondCall.calledWith(
      PathUtils.join(destPath, "favicons.sqlite"),
      NEW_SQLITE_PAGES_PER_STEP,
      NEW_SQLITE_STEP_DELAY_MS
    ),
    "backup called with favicons.sqlite to the destination path with the " +
      "right pages per step and step delay"
  );

  await maybeRemovePath(sourcePath);
  await maybeRemovePath(destPath);
  sandbox.restore();
});

/**
 * Tests that BackupResource.copyFiles will copy files from one directory to
 * another.
 */
add_task(async function test_copyFiles() {
  let sourcePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "BackupResource-source-test"
  );
  let destPath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "BackupResource-dest-test"
  );

  const testFiles = [
    { path: "file1.txt" },
    { path: ["some", "nested", "file", "file2.txt"] },
    { path: "file3.txt" },
  ];

  await createTestFiles(sourcePath, testFiles);

  await BackupResource.copyFiles(sourcePath, destPath, [
    "file1.txt",
    "some",
    "file3.txt",
    "does-not-exist.txt",
  ]);

  await assertFilesExist(destPath, testFiles);
  Assert.ok(
    !(await IOUtils.exists(PathUtils.join(destPath, "does-not-exist.txt"))),
    "does-not-exist.txt wasn't somehow written to."
  );

  await maybeRemovePath(sourcePath);
  await maybeRemovePath(destPath);
});
