/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "http://mochi.test:8888/";
const URL_COPY = URL + "#copy";

XPCOMUtils.defineLazyGetter(this, "Sanitizer", function () {
  let tmp = {};
  Cc["@mozilla.org/moz/jssubscript-loader;1"]
    .getService(Ci.mozIJSSubScriptLoader)
    .loadSubScript("chrome://browser/content/sanitize.js", tmp);
  return tmp.Sanitizer;
});

/**
 * These tests ensure that the thumbnail storage is working as intended.
 * Newly captured thumbnails should be saved as files and they should as well
 * be removed when the user sanitizes their history.
 */
function runTests() {
  yield clearHistory();
  yield createThumbnail();

  // Make sure Storage.copy() updates an existing file.
  PageThumbsStorage.copy(URL, URL_COPY);
  let copy = PageThumbsStorage.getFileForURL(URL_COPY);
  let mtime = copy.lastModifiedTime -= 60;

  PageThumbsStorage.copy(URL, URL_COPY);
  isnot(PageThumbsStorage.getFileForURL(URL_COPY).lastModifiedTime, mtime,
        "thumbnail file was updated");

  let file = PageThumbsStorage.getFileForURL(URL);
  let fileCopy = PageThumbsStorage.getFileForURL(URL_COPY);

  // Clear the browser history. Retry until the files are gone because Windows
  // locks them sometimes.
  while (file.exists() || fileCopy.exists()) {
    yield clearHistory();
  }

  yield createThumbnail();

  // Clear the last 10 minutes of browsing history.
  yield clearHistory(true);

  // Retry until the file is gone because Windows locks it sometimes.
  while (file.exists()) {
    // Re-add our URL to the history so that history observer's onDeleteURI()
    // is called again.
    let time = Date.now() * 1000;
    let trans = Ci.nsINavHistoryService.TRANSITION_LINK;
    PlacesUtils.history.addVisit(makeURI(URL), time, null, trans, false, 0);

    // Try again...
    yield clearHistory(true);
  }
}

function clearHistory(aUseRange) {
  let s = new Sanitizer();
  s.prefDomain = "privacy.cpd.";

  let prefs = gPrefService.getBranch(s.prefDomain);
  prefs.setBoolPref("history", true);
  prefs.setBoolPref("downloads", false);
  prefs.setBoolPref("cache", false);
  prefs.setBoolPref("cookies", false);
  prefs.setBoolPref("formdata", false);
  prefs.setBoolPref("offlineApps", false);
  prefs.setBoolPref("passwords", false);
  prefs.setBoolPref("sessions", false);
  prefs.setBoolPref("siteSettings", false);

  if (aUseRange) {
    let usec = Date.now() * 1000;
    s.range = [usec - 10 * 60 * 1000 * 1000, usec];
    s.ignoreTimespan = false;
  }

  s.sanitize();
  s.range = null;
  s.ignoreTimespan = true;

  executeSoon(next);
}

function createThumbnail() {
  addTab(URL, function () {
    whenFileExists(function () {
      gBrowser.removeTab(gBrowser.selectedTab);
      next();
    });
  });
}

function whenFileExists(aCallback) {
  let callback;
  let file = PageThumbsStorage.getFileForURL(URL);
  if (file.exists() && file.fileSize) {
    callback = aCallback;
  } else {
    callback = function () whenFileExists(aCallback);
  }

  executeSoon(callback);
}
