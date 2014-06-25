/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  let FileUtils =
    Cu.import("resource://gre/modules/FileUtils.jsm", {}).FileUtils;
  let DownloadLastDir =
    Cu.import("resource://gre/modules/DownloadLastDir.jsm", {}).DownloadLastDir;

  let tmpDir = FileUtils.getDir("TmpD", [], true);
  let dir1 = newDirectory();

  registerCleanupFunction(function () {
    Services.prefs.clearUserPref("browser.download.lastDir");
    dir1.remove(true);
  });

  function testOnWindow(aPrivate, aCallback) {
    whenNewWindowLoaded({private: aPrivate}, function(win) {
      let gDownloadLastDir = new DownloadLastDir(win);
      aCallback(win, gDownloadLastDir);
      gDownloadLastDir.cleanupPrivateFile();
      win.close();
    });
  }

  function checkDownloadLastDirInit(aWin, gDownloadLastDir, aCallback) {
    is(typeof gDownloadLastDir, "object",
       "gDownloadLastDir should be a valid object");
    is(gDownloadLastDir.file, null,
       "gDownloadLastDir.file should be null to start with");

    gDownloadLastDir.file = tmpDir;
    is(gDownloadLastDir.file.path, tmpDir.path,
       "LastDir should point to the temporary directory");
    isnot(gDownloadLastDir.file, tmpDir,
          "gDownloadLastDir.file should not be pointing to the tmpDir");

    gDownloadLastDir.file = 1; // not an nsIFile
    is(gDownloadLastDir.file, null, "gDownloadLastDir.file should be null");

    gDownloadLastDir.file = tmpDir;
    clearHistory();
    is(gDownloadLastDir.file, null, "gDownloadLastDir.file should be null");

    gDownloadLastDir.file = tmpDir;
    aCallback();
  }

  function checkDownloadLastDir(aWin, gDownloadLastDir, aLastDir, aUpdate, aCallback) {
    if (aUpdate)
      gDownloadLastDir.file = aLastDir;
    is(gDownloadLastDir.file.path, aLastDir.path,
       "gDownloadLastDir should point to the expected last directory");
    isnot(gDownloadLastDir.file, aLastDir,
          "gDownloadLastDir.file should not be pointing to the last directory");
    aCallback();
  }

  function checkDownloadLastDirNull(aWin, gDownloadLastDir, aCallback) {
    is(gDownloadLastDir.file, null, "gDownloadLastDir should be null");
    aCallback();
  }

  testOnWindow(false, function(win, downloadDir) {
    checkDownloadLastDirInit(win, downloadDir, function() {
      testOnWindow(true, function(win, downloadDir) {
        checkDownloadLastDir(win, downloadDir, tmpDir, false, function() {
          testOnWindow(false, function(win, downloadDir) {
            checkDownloadLastDir(win, downloadDir, tmpDir, false, function() {
              testOnWindow(true, function(win, downloadDir) {
                checkDownloadLastDir(win, downloadDir, dir1, true, function() {
                  testOnWindow(false, function(win, downloadDir) {
                    checkDownloadLastDir(win, downloadDir, tmpDir, false, function() {
                      testOnWindow(true, function(win, downloadDir) {
                        clearHistory();
                        checkDownloadLastDirNull(win, downloadDir, function() {
                          testOnWindow(false, function(win, downloadDir) {
                            checkDownloadLastDirNull(win, downloadDir, finish);
                          });
                        });
                      });
                    });
                  });
                });
              });
            });
          });
        });
      });
    });
  });
}
