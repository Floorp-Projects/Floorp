/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  var tmpScope = {};
  let downloadModule = {};
  Cu.import("resource://gre/modules/DownloadLastDir.jsm", downloadModule);
  Cu.import("resource://gre/modules/FileUtils.jsm", tmpScope);
  Cu.import("resource://gre/modules/Services.jsm");
  let FileUtils = tmpScope.FileUtils;
  let gDownloadLastDir = new downloadModule.DownloadLastDir(window);

  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);

  function clearHistory() {
    // simulate clearing the private data
    Services.obs.notifyObservers(null, "browser:purge-session-history", "");
  }

  is(typeof gDownloadLastDir, "object", "gDownloadLastDir should be a valid object");
  is(gDownloadLastDir.file, null, "gDownloadLastDir.file should be null to start with");
  let tmpDir = FileUtils.getDir("TmpD", [], true);
  let newDir = tmpDir.clone();

  registerCleanupFunction(function () {
    Services.prefs.clearUserPref("browser.privatebrowsing.keep_current_session");
    Services.prefs.clearUserPref("browser.download.lastDir");
    newDir.remove(true);
    gDownloadLastDir.cleanupPrivateFile();
    delete FileUtils;
  });

  newDir.append("testdir");
  newDir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0700);

  gDownloadLastDir.file = tmpDir;
  is(gDownloadLastDir.file.path, tmpDir.path, "LastDir should point to the temporary directory");
  isnot(gDownloadLastDir.file, tmpDir, "gDownloadLastDir.file should not be pointing to the tmpDir");

  gDownloadLastDir.file = 1; // not an nsIFile
  is(gDownloadLastDir.file, null, "gDownloadLastDir.file should be null");
  gDownloadLastDir.file = tmpDir;

  clearHistory();
  is(gDownloadLastDir.file, null, "gDownloadLastDir.file should be null");
  gDownloadLastDir.file = tmpDir;

  Services.prefs.setBoolPref("browser.privatebrowsing.keep_current_session", true);

  pb.privateBrowsingEnabled = true;
  is(gDownloadLastDir.file.path, tmpDir.path, "LastDir should point to the temporary directory");
  isnot(gDownloadLastDir.file, tmpDir, "gDownloadLastDir.file should not be pointing to the tmpDir");

  pb.privateBrowsingEnabled = false;
  is(gDownloadLastDir.file.path, tmpDir.path, "LastDir should point to the tmpDir");
  pb.privateBrowsingEnabled = true;

  gDownloadLastDir.file = newDir;
  is(gDownloadLastDir.file.path, newDir.path, "gDownloadLastDir should be modified in PB mode");
  isnot(gDownloadLastDir.file, newDir, "gDownloadLastDir should not point to the newDir");

  pb.privateBrowsingEnabled = false;
  is(gDownloadLastDir.file.path, tmpDir.path, "gDownloadLastDir should point to the earlier directory outside PB mode");
  isnot(gDownloadLastDir.file, tmpDir, "gDownloadLastDir should not be modifief outside PB mode");

  pb.privateBrowsingEnabled = true;
  isnot(gDownloadLastDir.file, null, "gDownloadLastDir should not be null inside PB mode");
  clearHistory();
  is(gDownloadLastDir.file, null, "gDownloadLastDir should be null after clearing history");

  pb.privateBrowsingEnabled = false;
  is(gDownloadLastDir.file, null, "gDownloadLastDir should be null outside PB mode");
}
