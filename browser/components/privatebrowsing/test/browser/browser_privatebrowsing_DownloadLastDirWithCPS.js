/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gTests;
function test() {
  waitForExplicitFinish();
  requestLongerTimeout(2);
  runTest().catch(ex => ok(false, ex));
}

/*
 * ================
 * Helper functions
 * ================
 */

function createWindow(aOptions) {
  return new Promise(resolve => whenNewWindowLoaded(aOptions, resolve));
}

function getFile(downloadLastDir, aURI) {
  return new Promise(resolve => downloadLastDir.getFileAsync(aURI, resolve));
}

function setFile(downloadLastDir, aURI, aValue) {
  downloadLastDir.setFile(aURI, aValue);
  return new Promise(resolve => executeSoon(resolve));
}

function clearHistoryAndWait() {
  clearHistory();
  return new Promise(resolve => executeSoon(_ => executeSoon(resolve)));
}

/*
 * ===================
 * Function with tests
 * ===================
 */

async function runTest() {
  let FileUtils =
    Cu.import("resource://gre/modules/FileUtils.jsm", {}).FileUtils;
  let DownloadLastDir =
    Cu.import("resource://gre/modules/DownloadLastDir.jsm", {}).DownloadLastDir;

  let tmpDir = FileUtils.getDir("TmpD", [], true);
  let dir1 = newDirectory();
  let dir2 = newDirectory();
  let dir3 = newDirectory();

  let uri1 = Services.io.newURI("http://test1.com/");
  let uri2 = Services.io.newURI("http://test2.com/");
  let uri3 = Services.io.newURI("http://test3.com/");
  let uri4 = Services.io.newURI("http://test4.com/");

  // cleanup functions registration
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("browser.download.lastDir.savePerSite");
    Services.prefs.clearUserPref("browser.download.lastDir");
    [dir1, dir2, dir3].forEach(dir => dir.remove(true));
    win.close();
    pbWin.close();
  });

  function checkDownloadLastDir(gDownloadLastDir, aLastDir) {
    is(gDownloadLastDir.file.path, aLastDir.path,
       "gDownloadLastDir should point to the expected last directory");
    return getFile(gDownloadLastDir, uri1);
  }

  function checkDownloadLastDirNull(gDownloadLastDir) {
    is(gDownloadLastDir.file, null, "gDownloadLastDir should be null");
    return getFile(gDownloadLastDir, uri1);
  }

  /*
   * ================================
   * Create a regular and a PB window
   * ================================
   */

  let win = await createWindow({private: false});
  let pbWin = await createWindow({private: true});

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
  await setFile(downloadLastDir, null, tmpDir);
  is(downloadLastDir.file.path, tmpDir.path,
     "LastDir should point to the tmpDir");
  isnot(downloadLastDir.file, tmpDir,
        "downloadLastDir.file should not be pointing to tmpDir");

  // set uri1 to dir1, all should now return dir1
  // also check that a new object is returned
  await setFile(downloadLastDir, uri1, dir1);
  is(downloadLastDir.file.path, dir1.path,
     "downloadLastDir should return dir1");
  isnot(downloadLastDir.file, dir1,
        "downloadLastDir.file should not return dir1");
  is((await getFile(downloadLastDir, uri1)).path, dir1.path,
     "uri1 should return dir1"); // set in CPS
  isnot((await getFile(downloadLastDir, uri1)), dir1,
        "getFile on uri1 should not return dir1");
  is((await getFile(downloadLastDir, uri2)).path, dir1.path,
     "uri2 should return dir1"); // fallback
  isnot((await getFile(downloadLastDir, uri2)), dir1,
        "getFile on uri2 should not return dir1");
  is((await getFile(downloadLastDir, uri3)).path, dir1.path,
     "uri3 should return dir1"); // fallback
  isnot((await getFile(downloadLastDir, uri3)), dir1,
        "getFile on uri3 should not return dir1");
  is((await getFile(downloadLastDir, uri4)).path, dir1.path,
     "uri4 should return dir1"); // fallback
  isnot((await getFile(downloadLastDir, uri4)), dir1,
        "getFile on uri4 should not return dir1");

  // set uri2 to dir2, all except uri1 should now return dir2
  await setFile(downloadLastDir, uri2, dir2);
  is(downloadLastDir.file.path, dir2.path,
     "downloadLastDir should point to dir2");
  is((await getFile(downloadLastDir, uri1)).path, dir1.path,
     "uri1 should return dir1"); // set in CPS
  is((await getFile(downloadLastDir, uri2)).path, dir2.path,
     "uri2 should return dir2"); // set in CPS
  is((await getFile(downloadLastDir, uri3)).path, dir2.path,
     "uri3 should return dir2"); // fallback
  is((await getFile(downloadLastDir, uri4)).path, dir2.path,
     "uri4 should return dir2"); // fallback

  // set uri3 to dir3, all except uri1 and uri2 should now return dir3
  await setFile(downloadLastDir, uri3, dir3);
  is(downloadLastDir.file.path, dir3.path,
     "downloadLastDir should point to dir3");
  is((await getFile(downloadLastDir, uri1)).path, dir1.path,
     "uri1 should return dir1"); // set in CPS
  is((await getFile(downloadLastDir, uri2)).path, dir2.path,
     "uri2 should return dir2"); // set in CPS
  is((await getFile(downloadLastDir, uri3)).path, dir3.path,
     "uri3 should return dir3"); // set in CPS
  is((await getFile(downloadLastDir, uri4)).path, dir3.path,
     "uri4 should return dir4"); // fallback

  // set uri1 to dir2, all except uri3 should now return dir2
  await setFile(downloadLastDir, uri1, dir2);
  is(downloadLastDir.file.path, dir2.path,
     "downloadLastDir should point to dir2");
  is((await getFile(downloadLastDir, uri1)).path, dir2.path,
     "uri1 should return dir2"); // set in CPS
  is((await getFile(downloadLastDir, uri2)).path, dir2.path,
     "uri2 should return dir2"); // set in CPS
  is((await getFile(downloadLastDir, uri3)).path, dir3.path,
     "uri3 should return dir3"); // set in CPS
  is((await getFile(downloadLastDir, uri4)).path, dir2.path,
     "uri4 should return dir2"); // fallback

  await clearHistoryAndWait();

  // check clearHistory removes all data
  is(downloadLastDir.file, null, "clearHistory removes all data");
  is((await getFile(downloadLastDir, uri1)), null, "uri1 should point to null");
  is((await getFile(downloadLastDir, uri2)), null, "uri2 should point to null");
  is((await getFile(downloadLastDir, uri3)), null, "uri3 should point to null");
  is((await getFile(downloadLastDir, uri4)), null, "uri4 should point to null");

  await setFile(downloadLastDir, null, tmpDir);

  // check data set outside PB mode is remembered
  is((await checkDownloadLastDir(pbDownloadLastDir, tmpDir)).path, tmpDir.path, "uri1 should return the expected last directory");
  is((await checkDownloadLastDir(downloadLastDir, tmpDir)).path, tmpDir.path, "uri1 should return the expected last directory");
  await clearHistoryAndWait();

  await setFile(downloadLastDir, uri1, dir1);

  // check data set using CPS outside PB mode is remembered
  is((await checkDownloadLastDir(pbDownloadLastDir, dir1)).path, dir1.path, "uri1 should return the expected last directory");
  is((await checkDownloadLastDir(downloadLastDir, dir1)).path, dir1.path, "uri1 should return the expected last directory");
  await clearHistoryAndWait();

  // check data set inside PB mode is forgotten
  await setFile(pbDownloadLastDir, null, tmpDir);

  is((await checkDownloadLastDir(pbDownloadLastDir, tmpDir)).path, tmpDir.path, "uri1 should return the expected last directory");
  is((await checkDownloadLastDirNull(downloadLastDir)), null, "uri1 should return the expected last directory");

  await clearHistoryAndWait();

  // check data set using CPS inside PB mode is forgotten
  await setFile(pbDownloadLastDir, uri1, dir1);

  is((await checkDownloadLastDir(pbDownloadLastDir, dir1)).path, dir1.path, "uri1 should return the expected last directory");
  is((await checkDownloadLastDirNull(downloadLastDir)), null, "uri1 should return the expected last directory");

  // check data set outside PB mode but changed inside is remembered correctly
  await setFile(downloadLastDir, uri1, dir1);
  await setFile(pbDownloadLastDir, uri1, dir2);
  is((await checkDownloadLastDir(pbDownloadLastDir, dir2)).path, dir2.path, "uri1 should return the expected last directory");
  is((await checkDownloadLastDir(downloadLastDir, dir1)).path, dir1.path, "uri1 should return the expected last directory");

  /*
   * ====================
   * Create new PB window
   * ====================
   */

  // check that the last dir store got cleared in a new PB window
  pbWin.close();
  // And give it time to close
  await new Promise(resolve => executeSoon(resolve));

  pbWin = await createWindow({private: true});
  pbDownloadLastDir = new DownloadLastDir(pbWin);

  is((await checkDownloadLastDir(pbDownloadLastDir, dir1)).path, dir1.path, "uri1 should return the expected last directory");

  await clearHistoryAndWait();

  // check clearHistory inside PB mode clears data outside PB mode
  await setFile(pbDownloadLastDir, uri1, dir2);

  await clearHistoryAndWait();

  is((await checkDownloadLastDirNull(downloadLastDir)), null, "uri1 should return the expected last directory");
  is((await checkDownloadLastDirNull(pbDownloadLastDir)), null, "uri1 should return the expected last directory");

  // check that disabling CPS works
  Services.prefs.setBoolPref("browser.download.lastDir.savePerSite", false);

  await setFile(downloadLastDir, uri1, dir1);
  is(downloadLastDir.file.path, dir1.path, "LastDir should be set to dir1");
  is((await getFile(downloadLastDir, uri1)).path, dir1.path, "uri1 should return dir1");
  is((await getFile(downloadLastDir, uri2)).path, dir1.path, "uri2 should return dir1");
  is((await getFile(downloadLastDir, uri3)).path, dir1.path, "uri3 should return dir1");
  is((await getFile(downloadLastDir, uri4)).path, dir1.path, "uri4 should return dir1");

  downloadLastDir.setFile(uri2, dir2);
  is(downloadLastDir.file.path, dir2.path, "LastDir should be set to dir2");
  is((await getFile(downloadLastDir, uri1)).path, dir2.path, "uri1 should return dir2");
  is((await getFile(downloadLastDir, uri2)).path, dir2.path, "uri2 should return dir2");
  is((await getFile(downloadLastDir, uri3)).path, dir2.path, "uri3 should return dir2");
  is((await getFile(downloadLastDir, uri4)).path, dir2.path, "uri4 should return dir2");

  Services.prefs.clearUserPref("browser.download.lastDir.savePerSite");

  // check that passing null to setFile clears the stored value
  await setFile(downloadLastDir, uri3, dir3);
  is((await getFile(downloadLastDir, uri3)).path, dir3.path, "LastDir should be set to dir3");
  await setFile(downloadLastDir, uri3, null);
  is((await getFile(downloadLastDir, uri3)), null, "uri3 should return null");

  await clearHistoryAndWait();

  finish();
}
