/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let gTests;
function test() {
  waitForExplicitFinish();
  requestLongerTimeout(2);
  gTests = runTest();
  gTests.next();
}

/*
 * ================
 * Helper functions
 * ================
 */

function moveAlong(aResult) {
  try {
    gTests.send(aResult);
  } catch (x if x instanceof StopIteration) {
    finish();
  }
}

function createWindow(aOptions) {
  whenNewWindowLoaded(aOptions, function(win) {
    moveAlong(win);
  });
}

function getFile(downloadLastDir, aURI) {
  downloadLastDir.getFileAsync(aURI, function(result) {
    moveAlong(result);
  });
}

function setFile(downloadLastDir, aURI, aValue) {
  downloadLastDir.setFile(aURI, aValue);
  executeSoon(moveAlong);
}

function clearHistoryAndWait() {
  clearHistory();
  executeSoon(function() executeSoon(moveAlong));
}

/*
 * ===================
 * Function with tests
 * ===================
 */

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
    win.close();
    pbWin.close();
  });

  function checkDownloadLastDir(gDownloadLastDir, aLastDir) {
    is(gDownloadLastDir.file.path, aLastDir.path,
       "gDownloadLastDir should point to the expected last directory");
    getFile(gDownloadLastDir, uri1);
  }

  function checkDownloadLastDirNull(gDownloadLastDir) {
    is(gDownloadLastDir.file, null, "gDownloadLastDir should be null");
    getFile(gDownloadLastDir, uri1);
  }

  /*
   * ================================
   * Create a regular and a PB window
   * ================================
   */

  let win = yield createWindow({private: false});
  let pbWin = yield createWindow({private: true});

  let downloadLastDir = new DownloadLastDir(win);
  let pbDownloadLastDir = new DownloadLastDir(pbWin);

  /*
   * ==================
   * Beginning of tests
   * ==================
   */

  is(typeof downloadLastDir, "object",
     "downloadLastDir should be a valid object");
  is(downloadLastDir.file, null,
     "LastDir pref should be null to start with");

  // set up last dir
  yield setFile(downloadLastDir, null, tmpDir);
  is(downloadLastDir.file.path, tmpDir.path,
     "LastDir should point to the tmpDir");
  isnot(downloadLastDir.file, tmpDir,
        "downloadLastDir.file should not be pointing to tmpDir");

  // set uri1 to dir1, all should now return dir1
  // also check that a new object is returned
  yield setFile(downloadLastDir, uri1, dir1);
  is(downloadLastDir.file.path, dir1.path,
     "downloadLastDir should return dir1");
  isnot(downloadLastDir.file, dir1,
        "downloadLastDir.file should not return dir1");
  is((yield getFile(downloadLastDir, uri1)).path, dir1.path,
     "uri1 should return dir1"); // set in CPS
  isnot((yield getFile(downloadLastDir, uri1)), dir1,
        "getFile on uri1 should not return dir1");
  is((yield getFile(downloadLastDir, uri2)).path, dir1.path,
     "uri2 should return dir1"); // fallback
  isnot((yield getFile(downloadLastDir, uri2)), dir1,
        "getFile on uri2 should not return dir1");
  is((yield getFile(downloadLastDir, uri3)).path, dir1.path,
     "uri3 should return dir1"); // fallback
  isnot((yield getFile(downloadLastDir, uri3)), dir1,
        "getFile on uri3 should not return dir1");
  is((yield getFile(downloadLastDir, uri4)).path, dir1.path,
     "uri4 should return dir1"); // fallback
  isnot((yield getFile(downloadLastDir, uri4)), dir1,
        "getFile on uri4 should not return dir1");

  // set uri2 to dir2, all except uri1 should now return dir2
  yield setFile(downloadLastDir, uri2, dir2);
  is(downloadLastDir.file.path, dir2.path,
     "downloadLastDir should point to dir2");
  is((yield getFile(downloadLastDir, uri1)).path, dir1.path,
     "uri1 should return dir1"); // set in CPS
  is((yield getFile(downloadLastDir, uri2)).path, dir2.path,
     "uri2 should return dir2"); // set in CPS
  is((yield getFile(downloadLastDir, uri3)).path, dir2.path,
     "uri3 should return dir2"); // fallback
  is((yield getFile(downloadLastDir, uri4)).path, dir2.path,
     "uri4 should return dir2"); // fallback

  // set uri3 to dir3, all except uri1 and uri2 should now return dir3
  yield setFile(downloadLastDir, uri3, dir3);
  is(downloadLastDir.file.path, dir3.path,
     "downloadLastDir should point to dir3");
  is((yield getFile(downloadLastDir, uri1)).path, dir1.path,
     "uri1 should return dir1"); // set in CPS
  is((yield getFile(downloadLastDir, uri2)).path, dir2.path,
     "uri2 should return dir2"); // set in CPS
  is((yield getFile(downloadLastDir, uri3)).path, dir3.path,
     "uri3 should return dir3"); // set in CPS
  is((yield getFile(downloadLastDir, uri4)).path, dir3.path,
     "uri4 should return dir4"); // fallback

  // set uri1 to dir2, all except uri3 should now return dir2
  yield setFile(downloadLastDir, uri1, dir2);
  is(downloadLastDir.file.path, dir2.path,
     "downloadLastDir should point to dir2");
  is((yield getFile(downloadLastDir, uri1)).path, dir2.path,
     "uri1 should return dir2"); // set in CPS
  is((yield getFile(downloadLastDir, uri2)).path, dir2.path,
     "uri2 should return dir2"); // set in CPS
  is((yield getFile(downloadLastDir, uri3)).path, dir3.path,
     "uri3 should return dir3"); // set in CPS
  is((yield getFile(downloadLastDir, uri4)).path, dir2.path,
     "uri4 should return dir2"); // fallback

  yield clearHistoryAndWait();

  // check clearHistory removes all data
  is(downloadLastDir.file, null, "clearHistory removes all data");
  //is(Services.contentPrefs.hasPref(uri1, "browser.download.lastDir", null),
  //   false, "LastDir preference should be absent");
  is((yield getFile(downloadLastDir, uri1)), null, "uri1 should point to null");
  is((yield getFile(downloadLastDir, uri2)), null, "uri2 should point to null");
  is((yield getFile(downloadLastDir, uri3)), null, "uri3 should point to null");
  is((yield getFile(downloadLastDir, uri4)), null, "uri4 should point to null");

  yield setFile(downloadLastDir, null, tmpDir);

  // check data set outside PB mode is remembered
  is((yield checkDownloadLastDir(pbDownloadLastDir, tmpDir)).path, tmpDir.path, "uri1 should return the expected last directory");
  is((yield checkDownloadLastDir(downloadLastDir, tmpDir)).path, tmpDir.path, "uri1 should return the expected last directory");
  yield clearHistoryAndWait();

  yield setFile(downloadLastDir, uri1, dir1);

  // check data set using CPS outside PB mode is remembered
  is((yield checkDownloadLastDir(pbDownloadLastDir, dir1)).path, dir1.path, "uri1 should return the expected last directory");
  is((yield checkDownloadLastDir(downloadLastDir, dir1)).path, dir1.path, "uri1 should return the expected last directory");
  yield clearHistoryAndWait();

  // check data set inside PB mode is forgotten
  yield setFile(pbDownloadLastDir, null, tmpDir);

  is((yield checkDownloadLastDir(pbDownloadLastDir, tmpDir)).path, tmpDir.path, "uri1 should return the expected last directory");
  is((yield checkDownloadLastDirNull(downloadLastDir)), null, "uri1 should return the expected last directory");

  yield clearHistoryAndWait();

  // check data set using CPS inside PB mode is forgotten
  yield setFile(pbDownloadLastDir, uri1, dir1);

  is((yield checkDownloadLastDir(pbDownloadLastDir, dir1)).path, dir1.path, "uri1 should return the expected last directory");
  is((yield checkDownloadLastDirNull(downloadLastDir)), null, "uri1 should return the expected last directory");

  // check data set outside PB mode but changed inside is remembered correctly
  yield setFile(downloadLastDir, uri1, dir1);
  yield setFile(pbDownloadLastDir, uri1, dir2);
  is((yield checkDownloadLastDir(pbDownloadLastDir, dir2)).path, dir2.path, "uri1 should return the expected last directory");
  is((yield checkDownloadLastDir(downloadLastDir, dir1)).path, dir1.path, "uri1 should return the expected last directory");

  /*
   * ====================
   * Create new PB window
   * ====================
   */

  // check that the last dir store got cleared in a new PB window
  pbWin.close();
  let pbWin = yield createWindow({private: true});
  let pbDownloadLastDir = new DownloadLastDir(pbWin);

  is((yield checkDownloadLastDir(pbDownloadLastDir, dir1)).path, dir1.path, "uri1 should return the expected last directory");

  yield clearHistoryAndWait();

  // check clearHistory inside PB mode clears data outside PB mode
  yield setFile(pbDownloadLastDir, uri1, dir2);

  yield clearHistoryAndWait();

  is((yield checkDownloadLastDirNull(downloadLastDir)), null, "uri1 should return the expected last directory");
  is((yield checkDownloadLastDirNull(pbDownloadLastDir)), null, "uri1 should return the expected last directory");

  // check that disabling CPS works
  Services.prefs.setBoolPref("browser.download.lastDir.savePerSite", false);

  yield setFile(downloadLastDir, uri1, dir1);
  is(downloadLastDir.file.path, dir1.path, "LastDir should be set to dir1");
  is((yield getFile(downloadLastDir, uri1)).path, dir1.path, "uri1 should return dir1");
  is((yield getFile(downloadLastDir, uri2)).path, dir1.path, "uri2 should return dir1");
  is((yield getFile(downloadLastDir, uri3)).path, dir1.path, "uri3 should return dir1");
  is((yield getFile(downloadLastDir, uri4)).path, dir1.path, "uri4 should return dir1");

  downloadLastDir.setFile(uri2, dir2);
  is(downloadLastDir.file.path, dir2.path, "LastDir should be set to dir2");
  is((yield getFile(downloadLastDir, uri1)).path, dir2.path, "uri1 should return dir2");
  is((yield getFile(downloadLastDir, uri2)).path, dir2.path, "uri2 should return dir2");
  is((yield getFile(downloadLastDir, uri3)).path, dir2.path, "uri3 should return dir2");
  is((yield getFile(downloadLastDir, uri4)).path, dir2.path, "uri4 should return dir2");

  Services.prefs.clearUserPref("browser.download.lastDir.savePerSite");

  // check that passing null to setFile clears the stored value
  yield setFile(downloadLastDir, uri3, dir3);
  is((yield getFile(downloadLastDir, uri3)).path, dir3.path, "LastDir should be set to dir3");
  yield setFile(downloadLastDir, uri3, null);
  is((yield getFile(downloadLastDir, uri3)), null, "uri3 should return null");

  yield clearHistoryAndWait();
}
