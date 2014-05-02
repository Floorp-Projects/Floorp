/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function whenNewWindowLoaded(aOptions, aCallback) {
  let win = OpenBrowserWindow(aOptions);
  let gotLoad = false;
  let gotActivate = Services.focus.activeWindow == win;

  function maybeRunCallback() {
    if (gotLoad && gotActivate) {
      executeSoon(function() { aCallback(win); });
    }
  }

  if (!gotActivate) {
    win.addEventListener("activate", function onActivate() {
      info("Got activate.");
      win.removeEventListener("activate", onActivate, false);
      gotActivate = true;
      maybeRunCallback();
    }, false);
  } else {
    info("Was activated.");
  }

  Services.obs.addObserver(function observer(aSubject, aTopic) {
    if (win == aSubject) {
      info("Delayed startup finished");
      Services.obs.removeObserver(observer, aTopic);
      gotLoad = true;
      maybeRunCallback();
    }
  }, "browser-delayed-startup-finished", false);

  return win;
}

function openWindow(aParent, aOptions, a3) {
  let { Promise: { defer } } = Components.utils.import("resource://gre/modules/Promise.jsm", {});
  let { promise, resolve } = defer();

  let win = aParent.OpenBrowserWindow(aOptions);

  win.addEventListener("load", function onLoad() {
    win.removeEventListener("load", onLoad, false);
    resolve(win);
  }, false);

  return promise;
}

function newDirectory() {
  let FileUtils =
    Cu.import("resource://gre/modules/FileUtils.jsm", {}).FileUtils;
  let tmpDir = FileUtils.getDir("TmpD", [], true);
  let dir = tmpDir.clone();
  dir.append("testdir");
  dir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  return dir;
}

function newFileInDirectory(aDir) {
  let FileUtils =
    Cu.import("resource://gre/modules/FileUtils.jsm", {}).FileUtils;
  let file = aDir.clone();
  file.append("testfile");
  file.createUnique(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_FILE);
  return file;
}

function clearHistory() {
  // simulate clearing the private data
  Services.obs.notifyObservers(null, "browser:purge-session-history", "");
}

function _initTest() {
  // Don't use about:home as the homepage for new windows
  Services.prefs.setIntPref("browser.startup.page", 0);
  registerCleanupFunction(function() Services.prefs.clearUserPref("browser.startup.page"));
}

_initTest();
