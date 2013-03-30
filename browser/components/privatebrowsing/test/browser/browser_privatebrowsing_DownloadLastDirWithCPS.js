/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let gTests;
function test() {
  waitForExplicitFinish();
  requestLongerTimeout(2);
  gTests = runTest();
  moveAlong();
}

function moveAlong() {
  try {
    gTests.next();
  } catch (x if x instanceof StopIteration) {
    finish();
  }
}

function runTest() {
  let FileUtils =
    Cu.import("resource://gre/modules/FileUtils.jsm", {}).FileUtils;
  let DownloadLastDir =
    Cu.import("resource://gre/modules/DownloadLastDir.jsm", {}).DownloadLastDir;

  let tmpDir = FileUtils.getDir("TmpD", [], true);
  let dir1 = newDirectory();
  let dir2 = newDirectory();
  let dir3 = newDirectory();

  let uri1 = Services.io.newURI("http://test1.com/", null, null);
  let uri2 = Services.io.newURI("http://test2.com/", null, null);
  let uri3 = Services.io.newURI("http://test3.com/", null, null);
  let uri4 = Services.io.newURI("http://test4.com/", null, null);

  // cleanup functions registration
  registerCleanupFunction(function () {
    Services.prefs.clearUserPref("browser.download.lastDir.savePerSite");
    Services.prefs.clearUserPref("browser.download.lastDir");
    [dir1, dir2, dir3].forEach(function(dir) dir.remove(true));
  });

  function testOnWindow(aPrivate, aCallback) {
    whenNewWindowLoaded({private: aPrivate}, function(win) {
      let gDownloadLastDir = new DownloadLastDir(win);
      aCallback(win, gDownloadLastDir);
      gDownloadLastDir.cleanupPrivateFile();
      win.close();
      executeSoon(moveAlong);
    });
  }

  function checkInit(aWin, gDownloadLastDir) {
    is(typeof gDownloadLastDir, "object",
       "gDownloadLastDir should be a valid object");
    is(gDownloadLastDir.file, null,
       "LastDir pref should be null to start with");

    // set up last dir
    gDownloadLastDir.setFile(null, tmpDir);
    is(gDownloadLastDir.file.path, tmpDir.path,
       "LastDir should point to the tmpDir");
    isnot(gDownloadLastDir.file, tmpDir,
          "gDownloadLastDir.file should not be pointing to tmpDir");

    // set uri1 to dir1, all should now return dir1
    // also check that a new object is returned
    gDownloadLastDir.setFile(uri1, dir1);
    is(gDownloadLastDir.file.path, dir1.path,
       "gDownloadLastDir should return dir1");
    isnot(gDownloadLastDir.file, dir1,
          "gDownloadLastDir.file should not return dir1");
    is(gDownloadLastDir.getFile(uri1).path, dir1.path,
       "uri1 should return dir1"); // set in CPS
    isnot(gDownloadLastDir.getFile(uri1), dir1,
          "getFile on uri1 should not return dir1");
    is(gDownloadLastDir.getFile(uri2).path, dir1.path,
       "uri2 should return dir1"); // fallback
    isnot(gDownloadLastDir.getFile(uri2), dir1,
          "getFile on uri2 should not return dir1");
    is(gDownloadLastDir.getFile(uri3).path, dir1.path,
       "uri3 should return dir1"); // fallback
    isnot(gDownloadLastDir.getFile(uri3), dir1,
          "getFile on uri3 should not return dir1");
    is(gDownloadLastDir.getFile(uri4).path, dir1.path,
       "uri4 should return dir1"); // fallback
    isnot(gDownloadLastDir.getFile(uri4), dir1,
          "getFile on uri4 should not return dir1");

    // set uri2 to dir2, all except uri1 should now return dir2
    gDownloadLastDir.setFile(uri2, dir2);
    is(gDownloadLastDir.file.path, dir2.path,
       "gDownloadLastDir should point to dir2");
    is(gDownloadLastDir.getFile(uri1).path, dir1.path,
       "uri1 should return dir1"); // set in CPS
    is(gDownloadLastDir.getFile(uri2).path, dir2.path,
       "uri2 should return dir2"); // set in CPS
    is(gDownloadLastDir.getFile(uri3).path, dir2.path,
       "uri3 should return dir2"); // fallback
    is(gDownloadLastDir.getFile(uri4).path, dir2.path,
       "uri4 should return dir2"); // fallback

    // set uri3 to dir3, all except uri1 and uri2 should now return dir3
    gDownloadLastDir.setFile(uri3, dir3);
    is(gDownloadLastDir.file.path, dir3.path,
       "gDownloadLastDir should point to dir3");
    is(gDownloadLastDir.getFile(uri1).path, dir1.path,
       "uri1 should return dir1"); // set in CPS
    is(gDownloadLastDir.getFile(uri2).path, dir2.path,
       "uri2 should return dir2"); // set in CPS
    is(gDownloadLastDir.getFile(uri3).path, dir3.path,
       "uri3 should return dir3"); // set in CPS
    is(gDownloadLastDir.getFile(uri4).path, dir3.path,
       "uri4 should return dir4"); // fallback

    // set uri1 to dir2, all except uri3 should now return dir2
    gDownloadLastDir.setFile(uri1, dir2);
    is(gDownloadLastDir.file.path, dir2.path,
       "gDownloadLastDir should point to dir2");
    is(gDownloadLastDir.getFile(uri1).path, dir2.path,
       "uri1 should return dir2"); // set in CPS
    is(gDownloadLastDir.getFile(uri2).path, dir2.path,
       "uri2 should return dir2"); // set in CPS
    is(gDownloadLastDir.getFile(uri3).path, dir3.path,
       "uri3 should return dir3"); // set in CPS
    is(gDownloadLastDir.getFile(uri4).path, dir2.path,
       "uri4 should return dir2"); // fallback

    // check clearHistory removes all data
    clearHistory();
    is(gDownloadLastDir.file, null, "clearHistory removes all data");
    is(Services.contentPrefs.hasPref(uri1, "browser.download.lastDir", null),
       false, "LastDir preference should be absent");
    is(gDownloadLastDir.getFile(uri1), null, "uri1 should point to null");
    is(gDownloadLastDir.getFile(uri2), null, "uri2 should point to null");
    is(gDownloadLastDir.getFile(uri3), null, "uri3 should point to null");
    is(gDownloadLastDir.getFile(uri4), null, "uri4 should point to null");
  }

  function checkDownloadLastDir(aWin, gDownloadLastDir, aLastDir) {
    is(gDownloadLastDir.file.path, aLastDir.path,
       "gDownloadLastDir should point to the expected last directory");
    is(gDownloadLastDir.getFile(uri1).path, aLastDir.path,
       "uri1 should return the expected last directory");
  }

  function checkDownloadLastDirNull(aWin, gDownloadLastDir) {
    is(gDownloadLastDir.file, null, "gDownloadLastDir should be null");
    is(gDownloadLastDir.getFile(uri1), null, "uri1 should return null");
  }

  function checkSetFile(gDownloadLastDir, aDir1, aDir2, aDir3) {
    // check that disabling CPS works
    Services.prefs.setBoolPref("browser.download.lastDir.savePerSite", false);

    gDownloadLastDir.setFile(uri1, aDir1);
    is(gDownloadLastDir.file.path, aDir1.path, "LastDir should be set to dir1");
    is(gDownloadLastDir.getFile(uri1).path, aDir1.path, "uri1 should return dir1");
    is(gDownloadLastDir.getFile(uri2).path, aDir1.path, "uri2 should return dir1");
    is(gDownloadLastDir.getFile(uri3).path, aDir1.path, "uri3 should return dir1");
    is(gDownloadLastDir.getFile(uri4).path, aDir1.path, "uri4 should return dir1");

    gDownloadLastDir.setFile(uri2, aDir2);
    is(gDownloadLastDir.file.path, aDir2.path, "LastDir should be set to dir2");
    is(gDownloadLastDir.getFile(uri1).path, aDir2.path, "uri1 should return dir2");
    is(gDownloadLastDir.getFile(uri2).path, aDir2.path, "uri2 should return dir2");
    is(gDownloadLastDir.getFile(uri3).path, aDir2.path, "uri3 should return dir2");
    is(gDownloadLastDir.getFile(uri4).path, aDir2.path, "uri4 should return dir2");

    Services.prefs.clearUserPref("browser.download.lastDir.savePerSite");

    // check that passing null to setFile clears the stored value
    gDownloadLastDir.setFile(uri3, aDir3);
    is(gDownloadLastDir.getFile(uri3).path, aDir3.path, "LastDir should be set to dir3");
    gDownloadLastDir.setFile(uri3, null);
    is(gDownloadLastDir.getFile(uri3), null, "uri3 should return null");
  }

  yield testOnWindow(false, function(win, downloadDir) {
    checkInit(win, downloadDir);
    downloadDir.setFile(null, tmpDir);
  });

  // check data set outside PB mode is remembered
  yield testOnWindow(true, function(win, downloadDir) {
    checkDownloadLastDir(win, downloadDir, tmpDir);
  });
  yield testOnWindow(false, function(win, downloadDir) {
    checkDownloadLastDir(win, downloadDir, tmpDir);
    clearHistory();
    downloadDir.setFile(uri1, dir1);
  });

  // check data set using CPS outside PB mode is remembered
  yield testOnWindow(true, function(win, downloadDir) {
    checkDownloadLastDir(win, downloadDir, dir1);
  });
  yield testOnWindow(false, function(win, downloadDir) {
    checkDownloadLastDir(win, downloadDir, dir1);
    clearHistory();
  });

  // check data set inside PB mode is forgotten
  yield testOnWindow(true, function(win, downloadDir) {
    downloadDir.setFile(null, tmpDir);
    checkDownloadLastDir(win, downloadDir, tmpDir);
  });
  yield testOnWindow(false, function(win, downloadDir) {
    checkDownloadLastDirNull(win, downloadDir);
    clearHistory();
  });

  // check data set using CPS inside PB mode is forgotten
  yield testOnWindow(true, function(win, downloadDir) {
    downloadDir.setFile(uri1, dir1);
    checkDownloadLastDir(win, downloadDir, dir1);
  });
  yield testOnWindow(false, function(win, downloadDir) {
    checkDownloadLastDirNull(win, downloadDir);
    clearHistory();
    downloadDir.setFile(uri1, dir1);
  });

  // check data set outside PB mode but changed inside is remembered correctly
  yield testOnWindow(true, function(win, downloadDir) {
    downloadDir.setFile(uri1, dir2);
    checkDownloadLastDir(win, downloadDir, dir2);
  });
  yield testOnWindow(false, function(win, downloadDir) {
    checkDownloadLastDir(win, downloadDir, dir1);
  });

  // check that the last dir store got cleared
  yield testOnWindow(true, function(win, downloadDir) {
    checkDownloadLastDir(win, downloadDir, dir1);
  });
  yield testOnWindow(false, function(win, downloadDir) {
    clearHistory();
  });

  // check clearHistory inside PB mode clears data outside PB mode
  yield testOnWindow(true, function(win, downloadDir) {
    downloadDir.setFile(uri1, dir2);
    clearHistory();
    checkDownloadLastDirNull(win, downloadDir);
  });
  yield testOnWindow(false, function(win, downloadDir) {
    checkDownloadLastDirNull(win, downloadDir);
    checkSetFile(downloadDir, dir1, dir2, dir3);
  });
}
