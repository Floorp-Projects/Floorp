/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var { PromiseUtils } = ChromeUtils.import(
  "resource://gre/modules/PromiseUtils.jsm"
);
var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(
  this,
  "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PlacesTestUtils",
  "resource://testing-common/PlacesTestUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "TestUtils",
  "resource://testing-common/TestUtils.jsm"
);

function whenNewWindowLoaded(aOptions, aCallback) {
  let win = OpenBrowserWindow(aOptions);
  let focused = SimpleTest.promiseFocus(win);
  let startupFinished = TestUtils.topicObserved(
    "browser-delayed-startup-finished",
    subject => subject == win
  ).then(() => win);
  Promise.all([focused, startupFinished]).then(results =>
    executeSoon(() => aCallback(results[1]))
  );

  return win;
}

function openWindow(aParent, aOptions) {
  let win = aParent.OpenBrowserWindow(aOptions);
  return TestUtils.topicObserved(
    "browser-delayed-startup-finished",
    subject => subject == win
  ).then(() => win);
}

/**
 * Opens a new private window and loads "about:privatebrowsing" there.
 */
async function openAboutPrivateBrowsing() {
  let win = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
    waitForTabURL: "about:privatebrowsing",
  });
  let tab = win.gBrowser.selectedBrowser;
  return { win, tab };
}

function newDirectory() {
  let FileUtils = ChromeUtils.import("resource://gre/modules/FileUtils.jsm", {})
    .FileUtils;
  let tmpDir = FileUtils.getDir("TmpD", [], true);
  let dir = tmpDir.clone();
  dir.append("testdir");
  dir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  return dir;
}

function newFileInDirectory(aDir) {
  let FileUtils = ChromeUtils.import("resource://gre/modules/FileUtils.jsm", {})
    .FileUtils;
  let file = aDir.clone();
  file.append("testfile");
  file.createUnique(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_FILE);
  return file;
}

function clearHistory() {
  // simulate clearing the private data
  Services.obs.notifyObservers(null, "browser:purge-session-history");
}

function _initTest() {
  // Don't use about:home as the homepage for new windows
  Services.prefs.setIntPref("browser.startup.page", 0);
  registerCleanupFunction(() =>
    Services.prefs.clearUserPref("browser.startup.page")
  );
}

_initTest();
