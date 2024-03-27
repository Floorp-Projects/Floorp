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
 * Tests that BackupService.getDirectorySize will get the total size of all the files in a directory and it's children in kilobytes.
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
