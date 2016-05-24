Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/DownloadLastDir.jsm");

/**
 * Tests how the browser remembers the last download folder
 * from download to download, with a particular emphasis
 * on how it behaves when private browsing windows open.
 */
add_task(function* test_downloads_last_dir_toggle() {
  let tmpDir = FileUtils.getDir("TmpD", [], true);
  let dir1 = newDirectory();

  registerCleanupFunction(function () {
    Services.prefs.clearUserPref("browser.download.lastDir");
    dir1.remove(true);
  });

  let win = yield BrowserTestUtils.openNewBrowserWindow();
  let gDownloadLastDir = new DownloadLastDir(win);
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
  yield BrowserTestUtils.closeWindow(win);

  info("Opening the first private window");
  yield testHelper({ private: true, expectedDir: tmpDir });
  info("Opening a non-private window");
  yield testHelper({ private: false, expectedDir: tmpDir });
  info("Opening a private window and setting download directory");
  yield testHelper({ private: true, setDir: dir1, expectedDir: dir1 });
  info("Opening a non-private window and checking download directory");
  yield testHelper({ private: false, expectedDir: tmpDir });
  info("Opening private window and clearing history");
  yield testHelper({ private: true, clearHistory: true, expectedDir: null });
  info("Opening a non-private window and checking download directory");
  yield testHelper({ private: true, expectedDir: null });
});

/**
 * Opens a new window and performs some test actions on it based
 * on the options object that have been passed in.
 *
 * @param options (Object)
 *        An object with the following properties:
 *
 *        clearHistory (bool, optional):
 *          Whether or not to simulate clearing session history.
 *          Defaults to false.
 *
 *        setDir (nsIFile, optional):
 *          An nsIFile for setting the last download directory.
 *          If not set, the load download directory is not changed.
 *
 *        expectedDir (nsIFile, expectedDir):
 *          An nsIFile for what we expect the last download directory
 *          should be. The nsIFile is not compared directly - only
 *          paths are compared. If expectedDir is not set, then the
 *          last download directory is expected to be null.
 *
 * @returns Promise
 */
function testHelper(options) {
  return new Task.spawn(function() {
    let win = yield BrowserTestUtils.openNewBrowserWindow(options);
    let gDownloadLastDir = new DownloadLastDir(win);

    if (options.clearHistory) {
      clearHistory();
    }

    if (options.setDir) {
      gDownloadLastDir.file = options.setDir;
    }

    let expectedDir = options.expectedDir;

    if (expectedDir) {
      is(gDownloadLastDir.file.path, expectedDir.path,
         "gDownloadLastDir should point to the expected last directory");
      isnot(gDownloadLastDir.file, expectedDir,
            "gDownloadLastDir.file should not be pointing to the last directory");
    } else {
      is(gDownloadLastDir.file, null, "gDownloadLastDir should be null");
    }

    gDownloadLastDir.cleanupPrivateFile();
    yield BrowserTestUtils.closeWindow(win);
  });
}
