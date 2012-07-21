/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  let tmpScope = {};
  let downloadModule = {};
  Cu.import("resource://gre/modules/DownloadLastDir.jsm", downloadModule);
  Cu.import("resource://gre/modules/FileUtils.jsm", tmpScope);
  let FileUtils = tmpScope.FileUtils;
  Cu.import("resource://gre/modules/Services.jsm");
  let MockFilePicker = SpecialPowers.MockFilePicker;
  let gDownloadLastDir = new downloadModule.DownloadLastDir(window);

  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);

  MockFilePicker.init();
  MockFilePicker.returnValue = Ci.nsIFilePicker.returnOK;

  //let stringBundleToRestore = ContentAreaUtils.stringBundle;
  let validateFileNameToRestore = validateFileName;

  Services.prefs.setBoolPref("browser.privatebrowsing.keep_current_session", true);
  let prefs = Services.prefs.getBranch("browser.download.");
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

  // cleanup function registration
  registerCleanupFunction(function () {
    Services.prefs.clearUserPref("browser.privatebrowsing.keep_current_session");
    Services.prefs.clearUserPref("browser.download.lastDir");
    [dir1, dir2, dir3].forEach(function(dir) dir.remove(true));
    MockFilePicker.cleanup();
    //ContentAreaUtils.stringBundle = stringBundleToRestore;
    validateFileName = validateFileNameToRestore;
    gDownloadLastDir.cleanupPrivateFile();
    delete FileUtils;
  });

  // Overwrite stringBundle to return an object masquerading as a string bundle
  /*delete ContentAreaUtils.stringBundle;
  ContentAreaUtils.stringBundle = {
    GetStringFromName: function() ""
  };*/

  // Overwrite validateFileName to validate everything
  validateFileName = function(foo) foo;

  let params = {
    //fpTitleKey: "test",
    fileInfo: new FileInfo("test.txt", "test.txt", "test", "txt", "http://mozilla.org/test.txt"),
    contentType: "text/plain",
    saveMode: SAVEMODE_FILEONLY,
    saveAsType: kSaveAsType_Complete,
    file: null
  };

  prefs.setComplexValue("lastDir", Ci.nsIFile, tmpDir);
  MockFilePicker.returnFiles = [file1];
  MockFilePicker.displayDirectory = null;
  ok(getTargetFile(params), "Show the file picker dialog with given params");
  // file picker should start with browser.download.lastDir
  is(MockFilePicker.displayDirectory.path, tmpDir.path, "file picker should start with browser.download.lastDir");
  // browser.download.lastDir should be modified before entering the private browsing mode
  is(prefs.getComplexValue("lastDir", Ci.nsIFile).path, dir1.path, "LastDir should be modified before entering PB mode");
  // gDownloadLastDir should be usable outside of the private browsing mode
  is(gDownloadLastDir.file.path, dir1.path, "gDownloadLastDir should be usable outside of the PB mode");

  pb.privateBrowsingEnabled = true;
  is(prefs.getComplexValue("lastDir", Ci.nsIFile).path, dir1.path, "LastDir should be that set before PB mode");
  MockFilePicker.returnFiles = [file2];
  MockFilePicker.displayDirectory = null;
  ok(getTargetFile(params), "Show the file picker dialog with the given params");
  // file picker should start with browser.download.lastDir as set before entering the private browsing mode
  is(MockFilePicker.displayDirectory.path, dir1.path, "File picker should start with LastDir set before entering PB mode");
  // browser.download.lastDir should not be modified inside the private browsing mode
  is(prefs.getComplexValue("lastDir", Ci.nsIFile).path, dir1.path, "LastDir should not be modified inside PB mode");
  // but gDownloadLastDir should be modified
  is(gDownloadLastDir.file.path, dir2.path, "gDownloadLastDir should be modified");

  pb.privateBrowsingEnabled = false;
  // gDownloadLastDir should be cleared after leaving the private browsing mode
  is(gDownloadLastDir.file.path, dir1.path, "gDownloadLastDir should be cleared after leaving PB mode");
  MockFilePicker.returnFiles = [file3];
  MockFilePicker.displayDirectory = null;
  ok(getTargetFile(params), "Show the file picker dialog with the given params");
  // file picker should start with browser.download.lastDir as set before entering the private browsing mode
  is(MockFilePicker.displayDirectory.path, dir1.path, "File picker should start with LastDir set before PB mode");
  // browser.download.lastDir should be modified after leaving the private browsing mode
  is(prefs.getComplexValue("lastDir", Ci.nsIFile).path, dir3.path, "LastDir should be modified after leaving PB mode");
  // gDownloadLastDir should be usable after leaving the private browsing mode
  is(gDownloadLastDir.file.path, dir3.path, "gDownloadLastDir should be usable after leaving PB mode");
}
