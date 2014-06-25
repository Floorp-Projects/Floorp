/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let {OS} = Cu.import("resource://gre/modules/osfile.jsm", {});
let {XPCOMUtils} = Cu.import("resource://gre/modules/XPCOMUtils.jsm", {});
let {Task} = Cu.import("resource://gre/modules/Task.jsm", {});
let {SessionWorker} = Cu.import("resource:///modules/sessionstore/SessionWorker.jsm", {});

let File = OS.File;
let Paths;
let SessionFile;

// We need a XULAppInfo to initialize SessionFile
let (XULAppInfo = {
  vendor: "Mozilla",
  name: "SessionRestoreTest",
  ID: "{230de50e-4cd1-11dc-8314-0800200c9a66}",
  version: "1",
  appBuildID: "2007010101",
  platformVersion: "",
  platformBuildID: "2007010101",
  inSafeMode: false,
  logConsoleErrors: true,
  OS: "XPCShell",
  XPCOMABI: "noarch-spidermonkey",

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIXULAppInfo,
    Ci.nsIXULRuntime,
  ])
}) {
  let XULAppInfoFactory = {
    createInstance: function (outer, iid) {
      if (outer != null)
        throw Cr.NS_ERROR_NO_AGGREGATION;
      return XULAppInfo.QueryInterface(iid);
    }
  };
  let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
  registrar.registerFactory(Components.ID("{fbfae60b-64a4-44ef-a911-08ceb70b9f31}"),
                            "XULAppInfo", "@mozilla.org/xre/app-info;1",
                            XULAppInfoFactory);
};

function run_test() {
  run_next_test();
}

add_task(function* init() {
  // Make sure that we have a profile before initializing SessionFile
  let profd = do_get_profile();
  SessionFile = Cu.import("resource:///modules/sessionstore/SessionFile.jsm", {}).SessionFile;
  Paths = SessionFile.Paths;


  let source = do_get_file("data/sessionstore_valid.js");
  source.copyTo(profd, "sessionstore.js");

  // Finish initialization of SessionFile
  yield SessionFile.read();
});

let pathStore;
let pathBackup;
let decoder;

function promise_check_exist(path, shouldExist) {
  return Task.spawn(function*() {
    do_print("Ensuring that " + path + (shouldExist?" exists":" does not exist"));
    if ((yield OS.File.exists(path)) != shouldExist) {
      throw new Error("File " + path + " should " + (shouldExist?"exist":"not exist"));
    }
  });
}

function promise_check_contents(path, expect) {
  return Task.spawn(function*() {
    do_print("Checking whether " + path + " has the right contents");
    let actual = yield OS.File.read(path, { encoding: "utf-8"});
    if (actual != expect) {
      throw new Error("File " + path + " should contain\n\t" + expect + "\nbut contains " + actual);
    }
  });
}

// Write to the store, and check that it creates:
// - $Path.recovery with the new data
// - $Path.nextUpgradeBackup with the old data
add_task(function* test_first_write_backup() {
  let initial_content = "initial content " + Math.random();
  let new_content = "test_1 " + Math.random();

  do_print("Before the first write, none of the files should exist");
  yield promise_check_exist(Paths.backups, false);

  yield File.makeDir(Paths.backups);
  yield File.writeAtomic(Paths.clean, initial_content, { encoding: "utf-8" });
  yield SessionFile.write(new_content);

  do_print("After first write, a few files should have been created");
  yield promise_check_exist(Paths.backups, true);
  yield promise_check_exist(Paths.clean, false);
  yield promise_check_exist(Paths.cleanBackup, true);
  yield promise_check_exist(Paths.recovery, true);
  yield promise_check_exist(Paths.recoveryBackup, false);
  yield promise_check_exist(Paths.nextUpgradeBackup, true);

  yield promise_check_contents(Paths.recovery, new_content);
  yield promise_check_contents(Paths.nextUpgradeBackup, initial_content);
});

// Write to the store again, and check that
// - $Path.clean is not written
// - $Path.recovery contains the new data
// - $Path.recoveryBackup contains the previous data
add_task(function* test_second_write_no_backup() {
  let new_content = "test_2 " + Math.random();
  let previous_backup_content = yield File.read(Paths.recovery, { encoding: "utf-8" });

  yield OS.File.remove(Paths.cleanBackup);

  yield SessionFile.write(new_content);

  yield promise_check_exist(Paths.backups, true);
  yield promise_check_exist(Paths.clean, false);
  yield promise_check_exist(Paths.cleanBackup, false);
  yield promise_check_exist(Paths.recovery, true);
  yield promise_check_exist(Paths.nextUpgradeBackup, true);

  yield promise_check_contents(Paths.recovery, new_content);
  yield promise_check_contents(Paths.recoveryBackup, previous_backup_content);
});

// Make sure that we create $Paths.clean and remove $Paths.recovery*
// upon shutdown
add_task(function* test_shutdown() {
  let output = "test_3 " + Math.random();

  yield File.writeAtomic(Paths.recovery, "I should disappear");
  yield File.writeAtomic(Paths.recoveryBackup, "I should also disappear");

  yield SessionWorker.post("write", [output, { isFinalWrite: true, performShutdownCleanup: true}]);

  do_check_false((yield File.exists(Paths.recovery)));
  do_check_false((yield File.exists(Paths.recoveryBackup)));
  let input = yield File.read(Paths.clean, { encoding: "utf-8"});
  do_check_eq(input, output);

});
