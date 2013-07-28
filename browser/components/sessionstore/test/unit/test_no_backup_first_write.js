/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let toplevel = this;
Cu.import("resource://gre/modules/osfile.jsm");

function run_test() {
  let profd = do_get_profile();
  Cu.import("resource:///modules/sessionstore/_SessionFile.jsm", toplevel);
  pathStore = OS.Path.join(OS.Constants.Path.profileDir, "sessionstore.js");
  pathBackup = OS.Path.join(OS.Constants.Path.profileDir, "sessionstore.bak");
  let source = do_get_file("data/sessionstore_valid.js");
  source.copyTo(profd, "sessionstore.js");
  run_next_test();
}

let pathStore;
let pathBackup;

// Write to the store first with |backupOnFirstWrite: false|,
// and make sure second write does not backup even with
// |backupOnFirstWrite: true|
add_task(function test_no_backup_on_second_write() {
  let content = "test_1";

  do_check_true(!(yield OS.File.exists(pathBackup)));
  yield _SessionFile.write(content, {backupOnFirstWrite: false});
  do_check_true(!(yield OS.File.exists(pathBackup)));

  yield _SessionFile.write(content, {backupOnFirstWrite: true});
  do_check_true(!(yield OS.File.exists(pathBackup)));
});
