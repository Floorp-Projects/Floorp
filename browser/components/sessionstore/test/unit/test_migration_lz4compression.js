"use strict";

const {XPCOMUtils} = Cu.import("resource://gre/modules/XPCOMUtils.jsm", {});
const {SessionWorker} = Cu.import("resource:///modules/sessionstore/SessionWorker.jsm", {});

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

function run_test() {
  run_next_test();
}

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
  return {windows: [{tabs: [{entries: [{url}], index: 1}]}]}
}

// Check whether the migration from .js to .jslz4 is correct.
add_task(async function test_migration() {
  // Make sure that we have a profile before initializing SessionFile.
  let profd = do_get_profile();
  SessionFile = Cu.import("resource:///modules/sessionstore/SessionFile.jsm", {}).SessionFile;
  Paths = SessionFile.Paths;

  let source = do_get_file("data/sessionstore_valid.js");
  source.copyTo(profd, "sessionstore.js");

  // Read the content of the session store file.
  let sessionStoreUncompressed = await OS.File.read(Paths.clean.replace("jsonlz4", "js"), {encoding: "utf-8"});
  let parsed = JSON.parse(sessionStoreUncompressed);

  // Read the session file with .js extension.
  let result = await SessionFile.read();

  // Check whether the result is what we wanted.
  equal(result.origin, "clean");
  equal(result.useOldExtension, true);
  Assert.deepEqual(result.parsed, parsed, "result.parsed contains expected data");

  // Initiate a write to ensure we write the compressed version.
  await SessionFile.write(parsed);
  await promise_check_exist(Paths.backups, true);
  await promise_check_exist(Paths.clean, false);
  await promise_check_exist(Paths.cleanBackup, true);
  await promise_check_exist(Paths.recovery, true);
  await promise_check_exist(Paths.recoveryBackup, false);
  await promise_check_exist(Paths.nextUpgradeBackup, true);
  // The deprecated $Path.clean should exist.
  await promise_check_exist(Paths.clean.replace("jsonlz4", "js"), true);

  await promise_check_contents(Paths.recovery, parsed);
});

add_task(async function test_startup_with_compressed_clean() {
  let state = {windows: []};
  let stateString = JSON.stringify(state);

  // Mare sure we have an empty profile dir.
  await SessionFile.wipe();

  // Populate session files to profile dir.
  await OS.File.writeAtomic(Paths.clean, stateString, {encoding: "utf-8", compression: "lz4"});
  await OS.File.makeDir(Paths.backups);
  await OS.File.writeAtomic(Paths.cleanBackup, stateString, {encoding: "utf-8", compression: "lz4"});

  // Initiate a read.
  let result = await SessionFile.read();

  // Make sure we read correct session file and its content.
  equal(result.origin, "clean");
  equal(result.useOldExtension, false);
  Assert.deepEqual(state, result.parsed, "result.parsed contains expected data");
});

add_task(async function test_empty_profile_dir() {
  // Make sure that we have empty profile dir.
  await SessionFile.wipe();
  await promise_check_exist(Paths.backups, false);
  await promise_check_exist(Paths.clean, false);
  await promise_check_exist(Paths.cleanBackup, false);
  await promise_check_exist(Paths.recovery, false);
  await promise_check_exist(Paths.recoveryBackup, false);
  await promise_check_exist(Paths.nextUpgradeBackup, false);
  await promise_check_exist(Paths.backups.replace("jsonlz4", "js"), false);
  await promise_check_exist(Paths.clean.replace("jsonlz4", "js"), false);
  await promise_check_exist(Paths.cleanBackup.replace("lz4", ""), false);
  await promise_check_exist(Paths.recovery.replace("jsonlz4", "js"), false);
  await promise_check_exist(Paths.recoveryBackup.replace("jsonlz4", "js"), false);
  await promise_check_exist(Paths.nextUpgradeBackup.replace("jsonlz4", "js"), false);

  // Initiate a read and make sure that we are in empty state.
  let result = await SessionFile.read();
  equal(result.origin, "empty");
  equal(result.noFilesFound, true);

  // Create a state to store.
  let state = {windows: []};
  await SessionWorker.post("write", [state, {isFinalWrite: true}]);

  // Check session files are created, but not deprecated ones.
  await promise_check_exist(Paths.clean, true);
  await promise_check_exist(Paths.clean.replace("jsonlz4", "js"), false);

  // Check session file' content is correct.
  await promise_check_contents(Paths.clean, state);
});
