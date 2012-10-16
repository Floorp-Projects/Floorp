/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  let FileUtils =
    Cu.import("resource://gre/modules/FileUtils.jsm", {}).FileUtils;
  let DownloadLastDir =
    Cu.import("resource://gre/modules/DownloadLastDir.jsm", {}).DownloadLastDir;
  let MockFilePicker = SpecialPowers.MockFilePicker;
  let launcher = {
    source: Services.io.newURI("http://test1.com/file", null, null)
  };

  MockFilePicker.init(window);
  MockFilePicker.returnValue = Ci.nsIFilePicker.returnOK;

  let prefs = Services.prefs.getBranch("browser.download.");
  let launcherDialog =
    Cc["@mozilla.org/helperapplauncherdialog;1"].
    getService(Ci.nsIHelperAppLauncherDialog);
  let tmpDir = FileUtils.getDir("TmpD", [], true);
  let dir1 = newDirectory();
  let dir2 = newDirectory();
  let dir3 = newDirectory();
  let file1 = newFileInDirectory(dir1);
  let file2 = newFileInDirectory(dir2);
  let file3 = newFileInDirectory(dir3);

  // cleanup functions registration
  registerCleanupFunction(function () {
    Services.prefs.clearUserPref("browser.download.lastDir");
    [dir1, dir2, dir3].forEach(function(dir) dir.remove(true));
    MockFilePicker.cleanup();
  });
  prefs.setComplexValue("lastDir", Ci.nsIFile, tmpDir);

  function testOnWindow(aPrivate, aCallback) {
    whenNewWindowLoaded({private: aPrivate}, function(win) {
      let gDownloadLastDir = new DownloadLastDir(win);
      aCallback(win, gDownloadLastDir);
      gDownloadLastDir.cleanupPrivateFile();
      win.close();
    });
  }

  function testDownloadDir(aWin, gDownloadLastDir, aFile, aDisplayDir, aLastDir,
                           aGlobalLastDir, aCallback) {
    let context = aWin.gBrowser.selectedBrowser.contentWindow;

    // Check lastDir preference.
    is(prefs.getComplexValue("lastDir", Ci.nsIFile).path, aDisplayDir.path,
       "LastDir should be the expected display dir");
    // Check gDownloadLastDir value.
    is(gDownloadLastDir.file.path, aDisplayDir.path,
       "gDownloadLastDir should be the expected display dir");

    MockFilePicker.returnFiles = [aFile];
    MockFilePicker.displayDirectory = null;
    let file =
      launcherDialog.promptForSaveToFile(launcher, context, null, null, null);
    ok(!!file, "promptForSaveToFile correctly returned a file");

    // File picker should start with expected display dir.
    is(MockFilePicker.displayDirectory.path, aDisplayDir.path,
      "File picker should start with browser.download.lastDir");
    // browser.download.lastDir should be modified on not private windows
    is(prefs.getComplexValue("lastDir", Ci.nsIFile).path, aLastDir.path,
       "LastDir should be the expected last dir");
    // gDownloadLastDir should be usable outside of private windows
    is(gDownloadLastDir.file.path, aGlobalLastDir.path,
       "gDownloadLastDir should be the expected global last dir");

    aCallback();
  }

  testOnWindow(false, function(win, downloadDir) {
    testDownloadDir(win, downloadDir, file1, tmpDir, dir1, dir1, function() {
      testOnWindow(true, function(win, downloadDir) {
        testDownloadDir(win, downloadDir, file2, dir1, dir1, dir2, function() {
          testOnWindow(false, function(win, downloadDir) {
            testDownloadDir(win, downloadDir, file3, dir1, dir3, dir3, finish);
          });
        });
      });
    });
  });
}
