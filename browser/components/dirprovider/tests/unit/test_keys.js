/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test_bookmarkhtml() {
  let bmarks = gProfD.clone();
  bmarks.append("bookmarks.html");

  let tbmarks = gDirSvc.get("BMarks", Ci.nsIFile);
  do_check_true(bmarks.equals(tbmarks));
}

function test_prefoverride() {
  let dir = gDirSvc.get("DefRt", Ci.nsIFile);
  dir.append("existing-profile-defaults.js");

  let tdir = gDirSvc.get("ExistingPrefOverride", Ci.nsIFile);
  do_check_true(dir.equals(tdir));
}

function run_test() {
  [test_bookmarkhtml,
   test_prefoverride
  ].forEach(function(f) {
    do_test_pending();
    print("Running test: " + f.name);
    f();
    do_test_finished();
  });
}
