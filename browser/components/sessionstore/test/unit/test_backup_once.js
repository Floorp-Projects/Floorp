/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var {XPCOMUtils} = Cu.import("resource://gre/modules/XPCOMUtils.jsm", {});
var {SessionWorker} = Cu.import("resource:///modules/sessionstore/SessionWorker.jsm", {});

var File = OS.File;
var Paths;
var SessionFile;

// We need a XULAppInfo to initialize SessionFile
Cu.import("resource://testing-common/AppInfo.jsm", this);
updateAppInfo({
  name: "SessionRestoreTest",
  ID: "{230de50e-4cd1-11dc-8314-0800200c9a66}",
  version: "1",
  platformVersion: "",
});

add_task(async function init() {
  // Make sure that we have a profile before initializing SessionFile
  let profd = do_get_profile();
  SessionFile = Cu.import("resource:///modules/sessionstore/SessionFile.jsm", {}).SessionFile;
  Paths = SessionFile.Paths;

  let source = do_get_file("data/sessionstore_valid.js");
  source.copyTo(profd, "sessionstore.js");
  await writeCompressedFile(Paths.clean.replace("jsonlz4", "js"), Paths.clean);

  // Finish initialization of SessionFile
  await SessionFile.read();
});

var pathStore;
var pathBackup;
var decoder;

function promise_check_exist(path, shouldExist) {
  return (async function() {
    do_print("Ensuring that " + path + (shouldExist ? " exists" : " does not exist"));
    if ((await OS.File.exists(path)) != shouldExist) {
      throw new Error("File " + path + " should " + (shouldExist ? "exist" : "not exist"));
    }
  })();
}

function promise_check_contents(path, expect) {
  return (async function() {
    do_print("Checking whether " + path + " has the right contents");
    let actual = await OS.File.read(path, { encoding: "utf-8", compression: "lz4" });
    Assert.deepEqual(JSON.parse(actual), expect, `File ${path} contains the expected data.`);
  })();
}

function generateFileContents(id) {
  let url = `http://example.com/test_backup_once#${id}_${Math.random()}`;
  return {windows: [{tabs: [{entries: [{url}], index: 1}]}]};
}

// Write to the store, and check that it creates:
// - $Path.recovery with the new data
// - $Path.nextUpgradeBackup with the old data
add_task(async function test_first_write_backup() {
  let initial_content = generateFileContents("initial");
  let new_content = generateFileContents("test_1");

  do_print("Before the first write, none of the files should exist");
  await promise_check_exist(Paths.backups, false);

  await File.makeDir(Paths.backups);
  await File.writeAtomic(Paths.clean, JSON.stringify(initial_content), { encoding: "utf-8", compression: "lz4" });
  await SessionFile.write(new_content);

  do_print("After first write, a few files should have been created");
  await promise_check_exist(Paths.backups, true);
  await promise_check_exist(Paths.clean, false);
  await promise_check_exist(Paths.cleanBackup, true);
  await promise_check_exist(Paths.recovery, true);
  await promise_check_exist(Paths.recoveryBackup, false);
  await promise_check_exist(Paths.nextUpgradeBackup, true);

  await promise_check_contents(Paths.recovery, new_content);
  await promise_check_contents(Paths.nextUpgradeBackup, initial_content);
});

// Write to the store again, and check that
// - $Path.clean is not written
// - $Path.recovery contains the new data
// - $Path.recoveryBackup contains the previous data
add_task(async function test_second_write_no_backup() {
  let new_content = generateFileContents("test_2");
  let previous_backup_content = await File.read(Paths.recovery, { encoding: "utf-8", compression: "lz4" });
  previous_backup_content = JSON.parse(previous_backup_content);

  await OS.File.remove(Paths.cleanBackup);

  await SessionFile.write(new_content);

  await promise_check_exist(Paths.backups, true);
  await promise_check_exist(Paths.clean, false);
  await promise_check_exist(Paths.cleanBackup, false);
  await promise_check_exist(Paths.recovery, true);
  await promise_check_exist(Paths.nextUpgradeBackup, true);

  await promise_check_contents(Paths.recovery, new_content);
  await promise_check_contents(Paths.recoveryBackup, previous_backup_content);
});

// Make sure that we create $Paths.clean and remove $Paths.recovery*
// upon shutdown
add_task(async function test_shutdown() {
  let output = generateFileContents("test_3");

  await File.writeAtomic(Paths.recovery, "I should disappear");
  await File.writeAtomic(Paths.recoveryBackup, "I should also disappear");

  await SessionWorker.post("write", [output, { isFinalWrite: true, performShutdownCleanup: true}]);

  Assert.equal(false, (await File.exists(Paths.recovery)));
  Assert.equal(false, (await File.exists(Paths.recoveryBackup)));
  await promise_check_contents(Paths.clean, output);
});
