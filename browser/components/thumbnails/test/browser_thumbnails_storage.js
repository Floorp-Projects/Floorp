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

  // create a thumbnail
  yield addTab(URL);
  yield whenFileExists();
  gBrowser.removeTab(gBrowser.selectedTab);

  // clear all browser history
  yield clearHistory();

  // create a thumbnail
  yield addTab(URL);
  yield whenFileExists();
  gBrowser.removeTab(gBrowser.selectedTab);

  // make sure copy() updates an existing file
  PageThumbsStorage.copy(URL, URL_COPY);
  let copy = PageThumbsStorage.getFileForURL(URL_COPY);
  let mtime = copy.lastModifiedTime -= 60;

  PageThumbsStorage.copy(URL, URL_COPY);
  isnot(PageThumbsStorage.getFileForURL(URL_COPY).lastModifiedTime, mtime,
        "thumbnail file was updated");

  // clear last 10 mins of history
  yield clearHistory(true);
  ok(!copy.exists(), "copy of thumbnail has been removed");
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
  }

  s.sanitize();
  s.range = null;

  executeSoon(function () {
    if (PageThumbsStorage.getFileForURL(URL).exists())
      clearHistory(aUseRange);
    else
      next();
  });
}

function whenFileExists() {
  let callback = whenFileExists;

  let file = PageThumbsStorage.getFileForURL(URL);
  if (file.exists() && file.fileSize)
    callback = next;

  executeSoon(callback);
}
