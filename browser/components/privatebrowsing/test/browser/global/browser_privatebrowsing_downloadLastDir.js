/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  let tempScope = {};
  let downloadModule = {};
  Cu.import("resource://gre/modules/Services.jsm");
  Cu.import("resource://gre/modules/DownloadLastDir.jsm", downloadModule);
  Cu.import("resource://gre/modules/FileUtils.jsm", tempScope);
  let FileUtils = tempScope.FileUtils;
  let MockFilePicker = SpecialPowers.MockFilePicker;
  let gDownloadLastDir = new downloadModule.DownloadLastDir(window);

  let launcher = {
    source: Services.io.newURI("http://test1.com/file", null, null)
  };
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);

  MockFilePicker.init(window);
   MockFilePicker.returnValue = Ci.nsIFilePicker.returnOK;

  Services.prefs.setBoolPref("browser.privatebrowsing.keep_current_session", true);
  let prefs = Services.prefs.getBranch("browser.download.");
  let launcherDialog = Cc["@mozilla.org/helperapplauncherdialog;1"].
                       getService(Ci.nsIHelperAppLauncherDialog);
  let tmpDir = FileUtils.getDir("TmpD", [], true);
  function newDirectory() {
    let dir = tmpDir.clone();
    dir.append("testdir");
    dir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0700);
    return dir;
  }
  function newFileInDirectory(dir) {
    let file = dir.clone();
    file.append("testfile");
    file.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0600);
    return file;
  }

  let dir1 = newDirectory();
  let dir2 = newDirectory();
  let dir3 = newDirectory();
  let file1 = newFileInDirectory(dir1);
  let file2 = newFileInDirectory(dir2);
  let file3 = newFileInDirectory(dir3);

  // cleanup functions registration
  registerCleanupFunction(function () {
    Services.prefs.clearUserPref("browser.privatebrowsing.keep_current_session");
    Services.prefs.clearUserPref("browser.download.lastDir");
    [dir1, dir2, dir3].forEach(function(dir) dir.remove(true));
    MockFilePicker.cleanup();
    gDownloadLastDir.cleanupPrivateFile();
    delete FileUtils;
  });

  let context = gBrowser.selectedBrowser.contentWindow;

  prefs.setComplexValue("lastDir", Ci.nsIFile, tmpDir);
  MockFilePicker.returnFiles = [file1];
  let file = launcherDialog.promptForSaveToFile(launcher, context, null, null, null);
  ok(!!file, "promptForSaveToFile correctly returned a file");
  // file picker should start with browser.download.lastDir
  is(MockFilePicker.displayDirectory.path, tmpDir.path, "File picker should start with browser.download.lastDir");
  // browser.download.lastDir should be modified before entering the private browsing mode
  is(prefs.getComplexValue("lastDir", Ci.nsIFile).path, dir1.path, "LastDir should be modified before entering the PB mode");
  // gDownloadLastDir should be usable outside of the private browsing mode
  is(gDownloadLastDir.file.path, dir1.path, "gDownloadLastDir should be usable outside of the PB mode");

  pb.privateBrowsingEnabled = true;
  is(prefs.getComplexValue("lastDir", Ci.nsIFile).path, dir1.path, "LastDir should be that set before entering PB mode");
  MockFilePicker.returnFiles = [file2];
  MockFilePicker.displayDirectory = null;
  file = launcherDialog.promptForSaveToFile(launcher, context, null, null, null);
  ok(!!file, "promptForSaveToFile correctly returned a file");
  // file picker should start with browser.download.lastDir as set before entering the private browsing mode
  is(MockFilePicker.displayDirectory.path, dir1.path, "Start with LastDir as set before entering the PB mode");
  // browser.download.lastDir should not be modified inside the private browsing mode
  is(prefs.getComplexValue("lastDir", Ci.nsIFile).path, dir1.path, "LastDir should not be modified inside the PB mode");
  // but gDownloadLastDir should be modified
  is(gDownloadLastDir.file.path, dir2.path, "gDownloadLastDir should be modified inside PB mode");

  pb.privateBrowsingEnabled = false;
  // gDownloadLastDir should be cleared after leaving the private browsing mode
  is(gDownloadLastDir.file.path, dir1.path, "gDownloadLastDir should be cleared after leaving the PB mode");
  MockFilePicker.returnFiles = [file3];
  MockFilePicker.displayDirectory = null;
  file = launcherDialog.promptForSaveToFile(launcher, context, null, null, null);
  ok(!!file, "promptForSaveToFile correctly returned a file");
  // file picker should start with browser.download.lastDir as set before entering the private browsing mode
  is(MockFilePicker.displayDirectory.path, dir1.path, "Start with LastDir as set before entering the PB mode");
  // browser.download.lastDir should be modified after leaving the private browsing mode
  is(prefs.getComplexValue("lastDir", Ci.nsIFile).path, dir3.path, "LastDir should be modified after leaving the PB mode");
  // gDownloadLastDir should be usable after leaving the private browsing mode
  is(gDownloadLastDir.file.path, dir3.path, "gDownloadLastDir should be usable after leaving the PB mode");
}
