/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// We need to run this test separately since DirectoryProvider persists BMarks

function run_test() {
  let dir = gProfD.clone();
  let tfile = writeTestFile(dir, "bookmarkfile.test");
  gPrefSvc.setCharPref("browser.bookmarks.file", tfile.path);

  let bmarks = gDirSvc.get("BMarks", Ci.nsIFile);
  do_check_true(tfile.equals(bmarks));
}
