/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const NOW = Date.now() * 1000;
const URL = "http://fake-site.com/";

let tmp = {};
Cu.import("resource:///modules/NewTabUtils.jsm", tmp);
Cc["@mozilla.org/moz/jssubscript-loader;1"]
  .getService(Ci.mozIJSSubScriptLoader)
  .loadSubScript("chrome://browser/content/sanitize.js", tmp);

let {NewTabUtils, Sanitizer} = tmp;

let bhist = Cc["@mozilla.org/browser/global-history;2"]
  .getService(Ci.nsIBrowserHistory);

function runTests() {
  clearHistory();
  fillHistory();
  yield addNewTabPageTab();

  is(getCell(0).site.url, URL, "first site is our fake site");

  whenPagesUpdated();
  yield clearHistory();

  ok(!getCell(0).site, "the fake site is gone");
}

function fillHistory() {
  let uri = makeURI(URL);
  for (let i = 59; i > 0; i--)
    bhist.addPageWithDetails(uri, "fake site", NOW - i * 60 * 1000000);
}

function clearHistory() {
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

  s.sanitize();
}
