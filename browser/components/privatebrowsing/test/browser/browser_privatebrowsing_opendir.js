/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Private Browsing Tests.
 *
 * The Initial Developer of the Original Code is
 * Ehsan Akhgari.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
