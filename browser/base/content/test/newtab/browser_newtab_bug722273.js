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
  yield fillHistory();
  yield addNewTabPageTab();

  is(getCell(0).site.url, URL, "first site is our fake site");

  whenPagesUpdated();
  yield clearHistory();

  ok(!getCell(0).site, "the fake site is gone");
}

function fillHistory() {
  let visits = [];
  for (let i = 59; i > 0; i--) {
    visits.push({
      visitDate: NOW - i * 60 * 1000000,
      transitionType: Ci.nsINavHistoryService.TRANSITION_LINK
    });
  }
  let place = {
    uri: makeURI(URL),
    title: "fake site",
    visits: visits
  };
  PlacesUtils.asyncHistory.updatePlaces(place, {
    handleError: function () do_throw("Unexpected error in adding visit."),
    handleResult: function () { },
    handleCompletion: function () TestRunner.next()
  });
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
