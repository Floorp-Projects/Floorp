/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */


let toplevel = this;
Cu.import("resource://gre/modules/osfile.jsm");

function run_test() {
  do_get_profile();
  Cu.import("resource:///modules/sessionstore/_SessionFile.jsm", toplevel);
  pathStore = OS.Path.join(OS.Constants.Path.profileDir, "sessionstore.js");
  run_next_test();
}

let pathStore;
function pathBackup(ext) {
  return OS.Path.join(OS.Constants.Path.profileDir, "sessionstore.bak" + ext);
}

// Ensure that things proceed smoothly if there is no file to back up
add_task(function test_nothing_to_backup() {
  yield _SessionFile.createBackupCopy("");
});

// Create a file, back it up, remove it
add_task(function test_do_backup() {
  let content = "test_1";
  let ext = ".upgrade_test_1";
  yield OS.File.writeAtomic(pathStore, content, {tmpPath: pathStore + ".tmp"});

  do_print("Ensuring that the backup is created");
  yield _SessionFile.createBackupCopy(ext);
  do_check_true((yield OS.File.exists(pathBackup(ext))));

  let data = yield OS.File.read(pathBackup(ext));
  do_check_eq((new TextDecoder()).decode(data), content);

  do_print("Ensuring that we can remove the backup");
  yield _SessionFile.removeBackupCopy(ext);
  do_check_false((yield OS.File.exists(pathBackup(ext))));
});

