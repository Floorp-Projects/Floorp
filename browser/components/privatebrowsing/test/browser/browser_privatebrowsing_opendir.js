/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the last open directory used inside the private
// browsing mode is not remembered after leaving that mode.

function test() {
  // initialization
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  let ds = Cc["@mozilla.org/file/directory_service;1"].
           getService(Ci.nsIProperties);
  let dir1 = ds.get("ProfD", Ci.nsIFile);
  let dir2 = ds.get("TmpD", Ci.nsIFile);
  let file = dir2.clone();
  file.append("pbtest.file");
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0600);

  const kPrefName = "browser.open.lastDir";

  function setupCleanSlate() {
    gLastOpenDirectory.reset();
    gPrefService.clearUserPref(kPrefName);
  }

  setupCleanSlate();

  // Test 1: general workflow test

  // initial checks
  ok(!gLastOpenDirectory.path,
     "Last open directory path should be initially empty");
  gLastOpenDirectory.path = dir2;
  is(gLastOpenDirectory.path.path, dir2.path,
     "The path should be successfully set");
  gLastOpenDirectory.path = null;
  is(gLastOpenDirectory.path.path, dir2.path,
     "The path should be not change when assigning it to null");
  gLastOpenDirectory.path = dir1;
  is(gLastOpenDirectory.path.path, dir1.path,
     "The path should be successfully outside of the private browsing mode");

  // enter private browsing mode
  pb.privateBrowsingEnabled = true;

  is(gLastOpenDirectory.path.path, dir1.path,
     "The path should not change when entering the private browsing mode");
  gLastOpenDirectory.path = dir2;
  is(gLastOpenDirectory.path.path, dir2.path,
     "The path should successfully change inside the private browsing mode");

  // leave private browsing mode
  pb.privateBrowsingEnabled = false;

  is(gLastOpenDirectory.path.path, dir1.path,
     "The path should be reset to the same path as before entering the private browsing mode");

  setupCleanSlate();

  // Test 2: the user first tries to open a file inside the private browsing mode

  pb.privateBrowsingEnabled = true;
  ok(!gLastOpenDirectory.path,
     "No original path should exist inside the private browsing mode");
  gLastOpenDirectory.path = dir1;
  is(gLastOpenDirectory.path.path, dir1.path, 
     "The path should be successfully set inside the private browsing mode");
  pb.privateBrowsingEnabled = false;
  ok(!gLastOpenDirectory.path,
     "The path set inside the private browsing mode should not leak when leaving that mode");

  setupCleanSlate();

  // Test 3: the last open directory is set from a previous session, it should be used
  // in normal mode

  gPrefService.setComplexValue(kPrefName, Ci.nsILocalFile, dir1);
  is(gLastOpenDirectory.path.path, dir1.path,
     "The pref set from last session should take effect outside the private browsing mode");

  setupCleanSlate();

  // Test 4: the last open directory is set from a previous session, it should be used
  // in private browsing mode mode

  gPrefService.setComplexValue(kPrefName, Ci.nsILocalFile, dir1);
  pb.privateBrowsingEnabled = true;
  is(gLastOpenDirectory.path.path, dir1.path,
     "The pref set from last session should take effect inside the private browsing mode");
  pb.privateBrowsingEnabled = false;
  is(gLastOpenDirectory.path.path, dir1.path,
     "The pref set from last session should remain in effect after leaving the private browsing mode");

  setupCleanSlate();

  // Test 5: setting the path to a file shouldn't work

  gLastOpenDirectory.path = file;
  ok(!gLastOpenDirectory.path,
     "Setting the path to a file shouldn't work when it's originally null");
  gLastOpenDirectory.path = dir1;
  gLastOpenDirectory.path = file;
  is(gLastOpenDirectory.path.path, dir1.path,
     "Setting the path to a file shouldn't work when it's not originally null");

  // cleanup
  file.remove(false);
}
