Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// Bug 453440 - Test the timespan-based logic of the sanitizer code
var now_uSec = Date.now() * 1000;

const kUsecPerMin = 60 * 1000000;

let tempScope = {};
Cc["@mozilla.org/moz/jssubscript-loader;1"].getService(Ci.mozIJSSubScriptLoader)
                                           .loadSubScript("chrome://browser/content/sanitize.js", tempScope);
let Sanitizer = tempScope.Sanitizer;

let FormHistory = (Components.utils.import("resource://gre/modules/FormHistory.jsm", {})).FormHistory;
let Downloads = (Components.utils.import("resource://gre/modules/Downloads.jsm", {})).Downloads;

function promiseFormHistoryRemoved() {
  let deferred = Promise.defer();
  Services.obs.addObserver(function onfh() {
    Services.obs.removeObserver(onfh, "satchel-storage-changed", false);
    deferred.resolve();
  }, "satchel-storage-changed", false);
  return deferred.promise;
}

function promiseDownloadRemoved(list) {
  let deferred = Promise.defer();

  let view = {
    onDownloadRemoved: function(download) {
      list.removeView(view);
      deferred.resolve();
    }
  };

  list.addView(view);
  
  return deferred.promise;
}

function test() {
  waitForExplicitFinish();

  Task.spawn(function() {
    yield setupDownloads();
    yield setupFormHistory();
    yield setupHistory();
    yield onHistoryReady();
  }).then(finish);
}

function countEntries(name, message, check) {
  let deferred = Promise.defer();

  var obj = {};
  if (name !== null)
    obj.fieldname = name;

  let count;
  FormHistory.count(obj, { handleResult: function (result) count = result,
                           handleError: function (error) {
                             do_throw("Error occurred searching form history: " + error);
                             deferred.reject(error)
                           },
                           handleCompletion: function (reason) {
                             if (!reason) {
                               check(count, message);
                               deferred.resolve();
                             }
                           },
                         });

  return deferred.promise;
}

function onHistoryReady() {
  var hoursSinceMidnight = new Date().getHours();
  var minutesSinceMidnight = hoursSinceMidnight * 60 + new Date().getMinutes();

  // Should test cookies here, but nsICookieManager/nsICookieService
  // doesn't let us fake creation times.  bug 463127
  
  let s = new Sanitizer();
  s.ignoreTimespan = false;
  s.prefDomain = "privacy.cpd.";
  var itemPrefs = gPrefService.getBranch(s.prefDomain);
  itemPrefs.setBoolPref("history", true);
  itemPrefs.setBoolPref("downloads", true);
  itemPrefs.setBoolPref("cache", false);
  itemPrefs.setBoolPref("cookies", false);
  itemPrefs.setBoolPref("formdata", true);
  itemPrefs.setBoolPref("offlineApps", false);
  itemPrefs.setBoolPref("passwords", false);
  itemPrefs.setBoolPref("sessions", false);
  itemPrefs.setBoolPref("siteSettings", false);

  let publicList = yield Downloads.getPublicDownloadList();
  let downloadPromise = promiseDownloadRemoved(publicList);

  // Clear 10 minutes ago
  s.range = [now_uSec - 10*60*1000000, now_uSec];
  s.sanitize();
  s.range = null;

  yield promiseFormHistoryRemoved();
  yield downloadPromise;

  ok(!(yield promiseIsURIVisited(makeURI("http://10minutes.com"))),
     "Pretend visit to 10minutes.com should now be deleted");
  ok((yield promiseIsURIVisited(makeURI("http://1hour.com"))),
     "Pretend visit to 1hour.com should should still exist");
  ok((yield promiseIsURIVisited(makeURI("http://1hour10minutes.com"))),
     "Pretend visit to 1hour10minutes.com should should still exist");
  ok((yield promiseIsURIVisited(makeURI("http://2hour.com"))),
     "Pretend visit to 2hour.com should should still exist");
  ok((yield promiseIsURIVisited(makeURI("http://2hour10minutes.com"))),
     "Pretend visit to 2hour10minutes.com should should still exist");
  ok((yield promiseIsURIVisited(makeURI("http://4hour.com"))),
     "Pretend visit to 4hour.com should should still exist");
  ok((yield promiseIsURIVisited(makeURI("http://4hour10minutes.com"))),
     "Pretend visit to 4hour10minutes.com should should still exist");
  if (minutesSinceMidnight > 10) {
    ok((yield promiseIsURIVisited(makeURI("http://today.com"))),
       "Pretend visit to today.com should still exist");
  }
  ok((yield promiseIsURIVisited(makeURI("http://before-today.com"))),
    "Pretend visit to before-today.com should still exist");

  let checkZero = function(num, message) { is(num, 0, message); }
  let checkOne = function(num, message) { is(num, 1, message); }

  yield countEntries("10minutes", "10minutes form entry should be deleted", checkZero);
  yield countEntries("1hour", "1hour form entry should still exist", checkOne);
  yield countEntries("1hour10minutes", "1hour10minutes form entry should still exist", checkOne);
  yield countEntries("2hour", "2hour form entry should still exist", checkOne);
  yield countEntries("2hour10minutes", "2hour10minutes form entry should still exist", checkOne);
  yield countEntries("4hour", "4hour form entry should still exist", checkOne);
  yield countEntries("4hour10minutes", "4hour10minutes form entry should still exist", checkOne);
  if (minutesSinceMidnight > 10)
    yield countEntries("today", "today form entry should still exist", checkOne);
  yield countEntries("b4today", "b4today form entry should still exist", checkOne);

  let downloads = yield publicList.getAll();
  ok(!(yield downloadExists(publicList, "fakefile-10-minutes")), "10 minute download should now be deleted");
  ok((yield downloadExists(publicList, "fakefile-1-hour")), "<1 hour download should still be present");
  ok((yield downloadExists(publicList, "fakefile-1-hour-10-minutes")), "1 hour 10 minute download should still be present");
  ok((yield downloadExists(publicList, "fakefile-old")), "Year old download should still be present");
  ok((yield downloadExists(publicList, "fakefile-2-hour")), "<2 hour old download should still be present");
  ok((yield downloadExists(publicList, "fakefile-2-hour-10-minutes")), "2 hour 10 minute download should still be present");
  ok((yield downloadExists(publicList, "fakefile-4-hour")), "<4 hour old download should still be present");
  ok((yield downloadExists(publicList, "fakefile-4-hour-10-minutes")), "4 hour 10 minute download should still be present");

  if (minutesSinceMidnight > 10)
    ok((yield downloadExists(publicList, "fakefile-today")), "'Today' download should still be present");

  downloadPromise = promiseDownloadRemoved(publicList);

  // Clear 1 hour
  Sanitizer.prefs.setIntPref("timeSpan", 1);
  s.sanitize();

  yield promiseFormHistoryRemoved();
  yield downloadPromise;

  ok(!(yield promiseIsURIVisited(makeURI("http://1hour.com"))),
     "Pretend visit to 1hour.com should now be deleted");
  ok((yield promiseIsURIVisited(makeURI("http://1hour10minutes.com"))),
     "Pretend visit to 1hour10minutes.com should should still exist");
  ok((yield promiseIsURIVisited(makeURI("http://2hour.com"))),
     "Pretend visit to 2hour.com should should still exist");
  ok((yield promiseIsURIVisited(makeURI("http://2hour10minutes.com"))),
     "Pretend visit to 2hour10minutes.com should should still exist");
  ok((yield promiseIsURIVisited(makeURI("http://4hour.com"))),
     "Pretend visit to 4hour.com should should still exist");
  ok((yield promiseIsURIVisited(makeURI("http://4hour10minutes.com"))),
     "Pretend visit to 4hour10minutes.com should should still exist");
  if (hoursSinceMidnight > 1) {
    ok((yield promiseIsURIVisited(makeURI("http://today.com"))),
       "Pretend visit to today.com should still exist");
  }
  ok((yield promiseIsURIVisited(makeURI("http://before-today.com"))),
    "Pretend visit to before-today.com should still exist");

  yield countEntries("1hour", "1hour form entry should be deleted", checkZero);
  yield countEntries("1hour10minutes", "1hour10minutes form entry should still exist", checkOne);
  yield countEntries("2hour", "2hour form entry should still exist", checkOne);
  yield countEntries("2hour10minutes", "2hour10minutes form entry should still exist", checkOne);
  yield countEntries("4hour", "4hour form entry should still exist", checkOne);
  yield countEntries("4hour10minutes", "4hour10minutes form entry should still exist", checkOne);
  if (hoursSinceMidnight > 1)
    yield countEntries("today", "today form entry should still exist", checkOne);
  yield countEntries("b4today", "b4today form entry should still exist", checkOne);

  ok(!(yield downloadExists(publicList, "fakefile-1-hour")), "<1 hour download should now be deleted");
  ok((yield downloadExists(publicList, "fakefile-1-hour-10-minutes")), "1 hour 10 minute download should still be present");
  ok((yield downloadExists(publicList, "fakefile-old")), "Year old download should still be present");
  ok((yield downloadExists(publicList, "fakefile-2-hour")), "<2 hour old download should still be present");
  ok((yield downloadExists(publicList, "fakefile-2-hour-10-minutes")), "2 hour 10 minute download should still be present");
  ok((yield downloadExists(publicList, "fakefile-4-hour")), "<4 hour old download should still be present");
  ok((yield downloadExists(publicList, "fakefile-4-hour-10-minutes")), "4 hour 10 minute download should still be present");

  if (hoursSinceMidnight > 1)
    ok((yield downloadExists(publicList, "fakefile-today")), "'Today' download should still be present");
  
  downloadPromise = promiseDownloadRemoved(publicList);

  // Clear 1 hour 10 minutes
  s.range = [now_uSec - 70*60*1000000, now_uSec];
  s.sanitize();
  s.range = null;

  yield promiseFormHistoryRemoved();
  yield downloadPromise;

  ok(!(yield promiseIsURIVisited(makeURI("http://1hour10minutes.com"))),
     "Pretend visit to 1hour10minutes.com should now be deleted");
  ok((yield promiseIsURIVisited(makeURI("http://2hour.com"))),
     "Pretend visit to 2hour.com should should still exist");
  ok((yield promiseIsURIVisited(makeURI("http://2hour10minutes.com"))),
     "Pretend visit to 2hour10minutes.com should should still exist");
  ok((yield promiseIsURIVisited(makeURI("http://4hour.com"))),
     "Pretend visit to 4hour.com should should still exist");
  ok((yield promiseIsURIVisited(makeURI("http://4hour10minutes.com"))),
     "Pretend visit to 4hour10minutes.com should should still exist");
  if (minutesSinceMidnight > 70) {
    ok((yield promiseIsURIVisited(makeURI("http://today.com"))),
       "Pretend visit to today.com should still exist");
  }
  ok((yield promiseIsURIVisited(makeURI("http://before-today.com"))),
    "Pretend visit to before-today.com should still exist");

  yield countEntries("1hour10minutes", "1hour10minutes form entry should be deleted", checkZero);
  yield countEntries("2hour", "2hour form entry should still exist", checkOne);
  yield countEntries("2hour10minutes", "2hour10minutes form entry should still exist", checkOne);
  yield countEntries("4hour", "4hour form entry should still exist", checkOne);
  yield countEntries("4hour10minutes", "4hour10minutes form entry should still exist", checkOne);
  if (minutesSinceMidnight > 70)
    yield countEntries("today", "today form entry should still exist", checkOne);
  yield countEntries("b4today", "b4today form entry should still exist", checkOne);

  ok(!(yield downloadExists(publicList, "fakefile-1-hour-10-minutes")), "1 hour 10 minute old download should now be deleted");
  ok((yield downloadExists(publicList, "fakefile-old")), "Year old download should still be present");
  ok((yield downloadExists(publicList, "fakefile-2-hour")), "<2 hour old download should still be present");
  ok((yield downloadExists(publicList, "fakefile-2-hour-10-minutes")), "2 hour 10 minute download should still be present");
  ok((yield downloadExists(publicList, "fakefile-4-hour")), "<4 hour old download should still be present");
  ok((yield downloadExists(publicList, "fakefile-4-hour-10-minutes")), "4 hour 10 minute download should still be present");
  if (minutesSinceMidnight > 70)
    ok((yield downloadExists(publicList, "fakefile-today")), "'Today' download should still be present");

  downloadPromise = promiseDownloadRemoved(publicList);

  // Clear 2 hours
  Sanitizer.prefs.setIntPref("timeSpan", 2);
  s.sanitize();

  yield promiseFormHistoryRemoved();
  yield downloadPromise;

  ok(!(yield promiseIsURIVisited(makeURI("http://2hour.com"))),
     "Pretend visit to 2hour.com should now be deleted");
  ok((yield promiseIsURIVisited(makeURI("http://2hour10minutes.com"))),
     "Pretend visit to 2hour10minutes.com should should still exist");
  ok((yield promiseIsURIVisited(makeURI("http://4hour.com"))),
     "Pretend visit to 4hour.com should should still exist");
  ok((yield promiseIsURIVisited(makeURI("http://4hour10minutes.com"))),
     "Pretend visit to 4hour10minutes.com should should still exist");
  if (hoursSinceMidnight > 2) {
    ok((yield promiseIsURIVisited(makeURI("http://today.com"))),
       "Pretend visit to today.com should still exist");
  }
  ok((yield promiseIsURIVisited(makeURI("http://before-today.com"))),
    "Pretend visit to before-today.com should still exist");

  yield countEntries("2hour", "2hour form entry should be deleted", checkZero);
  yield countEntries("2hour10minutes", "2hour10minutes form entry should still exist", checkOne);
  yield countEntries("4hour", "4hour form entry should still exist", checkOne);
  yield countEntries("4hour10minutes", "4hour10minutes form entry should still exist", checkOne);
  if (hoursSinceMidnight > 2)
    yield countEntries("today", "today form entry should still exist", checkOne);
  yield countEntries("b4today", "b4today form entry should still exist", checkOne);

  ok(!(yield downloadExists(publicList, "fakefile-2-hour")), "<2 hour old download should now be deleted");
  ok((yield downloadExists(publicList, "fakefile-old")), "Year old download should still be present");
  ok((yield downloadExists(publicList, "fakefile-2-hour-10-minutes")), "2 hour 10 minute download should still be present");
  ok((yield downloadExists(publicList, "fakefile-4-hour")), "<4 hour old download should still be present");
  ok((yield downloadExists(publicList, "fakefile-4-hour-10-minutes")), "4 hour 10 minute download should still be present");
  if (hoursSinceMidnight > 2)
    ok((yield downloadExists(publicList, "fakefile-today")), "'Today' download should still be present");
  
  downloadPromise = promiseDownloadRemoved(publicList);

  // Clear 2 hours 10 minutes
  s.range = [now_uSec - 130*60*1000000, now_uSec];
  s.sanitize();
  s.range = null;

  yield promiseFormHistoryRemoved();
  yield downloadPromise;

  ok(!(yield promiseIsURIVisited(makeURI("http://2hour10minutes.com"))),
     "Pretend visit to 2hour10minutes.com should now be deleted");
  ok((yield promiseIsURIVisited(makeURI("http://4hour.com"))),
     "Pretend visit to 4hour.com should should still exist");
  ok((yield promiseIsURIVisited(makeURI("http://4hour10minutes.com"))),
     "Pretend visit to 4hour10minutes.com should should still exist");
  if (minutesSinceMidnight > 130) {
    ok((yield promiseIsURIVisited(makeURI("http://today.com"))),
       "Pretend visit to today.com should still exist");
  }
  ok((yield promiseIsURIVisited(makeURI("http://before-today.com"))),
    "Pretend visit to before-today.com should still exist");

  yield countEntries("2hour10minutes", "2hour10minutes form entry should be deleted", checkZero);
  yield countEntries("4hour", "4hour form entry should still exist", checkOne);
  yield countEntries("4hour10minutes", "4hour10minutes form entry should still exist", checkOne);
  if (minutesSinceMidnight > 130)
    yield countEntries("today", "today form entry should still exist", checkOne);
  yield countEntries("b4today", "b4today form entry should still exist", checkOne);

  ok(!(yield downloadExists(publicList, "fakefile-2-hour-10-minutes")), "2 hour 10 minute old download should now be deleted");
  ok((yield downloadExists(publicList, "fakefile-4-hour")), "<4 hour old download should still be present");
  ok((yield downloadExists(publicList, "fakefile-4-hour-10-minutes")), "4 hour 10 minute download should still be present");
  ok((yield downloadExists(publicList, "fakefile-old")), "Year old download should still be present");
  if (minutesSinceMidnight > 130)
    ok((yield downloadExists(publicList, "fakefile-today")), "'Today' download should still be present");

  downloadPromise = promiseDownloadRemoved(publicList);

  // Clear 4 hours
  Sanitizer.prefs.setIntPref("timeSpan", 3);
  s.sanitize();

  yield promiseFormHistoryRemoved();
  yield downloadPromise;

  ok(!(yield promiseIsURIVisited(makeURI("http://4hour.com"))),
     "Pretend visit to 4hour.com should now be deleted");
  ok((yield promiseIsURIVisited(makeURI("http://4hour10minutes.com"))),
     "Pretend visit to 4hour10minutes.com should should still exist");
  if (hoursSinceMidnight > 4) {
    ok((yield promiseIsURIVisited(makeURI("http://today.com"))),
       "Pretend visit to today.com should still exist");
  }
  ok((yield promiseIsURIVisited(makeURI("http://before-today.com"))),
    "Pretend visit to before-today.com should still exist");

  yield countEntries("4hour", "4hour form entry should be deleted", checkZero);
  yield countEntries("4hour10minutes", "4hour10minutes form entry should still exist", checkOne);
  if (hoursSinceMidnight > 4)
    yield countEntries("today", "today form entry should still exist", checkOne);
  yield countEntries("b4today", "b4today form entry should still exist", checkOne);

  ok(!(yield downloadExists(publicList, "fakefile-4-hour")), "<4 hour old download should now be deleted");
  ok((yield downloadExists(publicList, "fakefile-4-hour-10-minutes")), "4 hour 10 minute download should still be present");
  ok((yield downloadExists(publicList, "fakefile-old")), "Year old download should still be present");
  if (hoursSinceMidnight > 4)
    ok((yield downloadExists(publicList, "fakefile-today")), "'Today' download should still be present");

  downloadPromise = promiseDownloadRemoved(publicList);

  // Clear 4 hours 10 minutes
  s.range = [now_uSec - 250*60*1000000, now_uSec];
  s.sanitize();
  s.range = null;

  yield promiseFormHistoryRemoved();
  yield downloadPromise;

  ok(!(yield promiseIsURIVisited(makeURI("http://4hour10minutes.com"))),
     "Pretend visit to 4hour10minutes.com should now be deleted");
  if (minutesSinceMidnight > 250) {
    ok((yield promiseIsURIVisited(makeURI("http://today.com"))),
       "Pretend visit to today.com should still exist");
  }
  ok((yield promiseIsURIVisited(makeURI("http://before-today.com"))),
    "Pretend visit to before-today.com should still exist");

  yield countEntries("4hour10minutes", "4hour10minutes form entry should be deleted", checkZero);
  if (minutesSinceMidnight > 250)
    yield countEntries("today", "today form entry should still exist", checkOne);
  yield countEntries("b4today", "b4today form entry should still exist", checkOne);
  
  ok(!(yield downloadExists(publicList, "fakefile-4-hour-10-minutes")), "4 hour 10 minute download should now be deleted");
  ok((yield downloadExists(publicList, "fakefile-old")), "Year old download should still be present");
  if (minutesSinceMidnight > 250)
    ok((yield downloadExists(publicList, "fakefile-today")), "'Today' download should still be present");

  downloadPromise = promiseDownloadRemoved(publicList);

  // Clear Today
  Sanitizer.prefs.setIntPref("timeSpan", 4);
  s.sanitize();

  yield promiseFormHistoryRemoved();
  yield downloadPromise;

  // Be careful.  If we add our objectss just before midnight, and sanitize
  // runs immediately after, they won't be expired.  This is expected, but
  // we should not test in that case.  We cannot just test for opposite
  // condition because we could cross midnight just one moment after we
  // cache our time, then we would have an even worse random failure.
  var today = isToday(new Date(now_uSec/1000));
  if (today) {
    ok(!(yield promiseIsURIVisited(makeURI("http://today.com"))),
       "Pretend visit to today.com should now be deleted");

    yield countEntries("today", "today form entry should be deleted", checkZero);
    ok(!(yield downloadExists(publicList, "fakefile-today")), "'Today' download should now be deleted");
  }

  ok((yield promiseIsURIVisited(makeURI("http://before-today.com"))),
     "Pretend visit to before-today.com should still exist");
  yield countEntries("b4today", "b4today form entry should still exist", checkOne);
  ok((yield downloadExists(publicList, "fakefile-old")), "Year old download should still be present");

  downloadPromise = promiseDownloadRemoved(publicList);

  // Choose everything
  Sanitizer.prefs.setIntPref("timeSpan", 0);
  s.sanitize();

  yield promiseFormHistoryRemoved();
  yield downloadPromise;

  ok(!(yield promiseIsURIVisited(makeURI("http://before-today.com"))),
     "Pretend visit to before-today.com should now be deleted");

  yield countEntries("b4today", "b4today form entry should be deleted", checkZero);

  ok(!(yield downloadExists(publicList, "fakefile-old")), "Year old download should now be deleted");
}

function setupHistory() {
  let deferred = Promise.defer();

  let places = [];

  function addPlace(aURI, aTitle, aVisitDate) {
    places.push({
      uri: aURI,
      title: aTitle,
      visits: [{
        visitDate: aVisitDate,
        transitionType: Ci.nsINavHistoryService.TRANSITION_LINK
      }]
    });
  }

  addPlace(makeURI("http://10minutes.com/"), "10 minutes ago", now_uSec - 10 * kUsecPerMin);
  addPlace(makeURI("http://1hour.com/"), "Less than 1 hour ago", now_uSec - 45 * kUsecPerMin);
  addPlace(makeURI("http://1hour10minutes.com/"), "1 hour 10 minutes ago", now_uSec - 70 * kUsecPerMin);
  addPlace(makeURI("http://2hour.com/"), "Less than 2 hours ago", now_uSec - 90 * kUsecPerMin);
  addPlace(makeURI("http://2hour10minutes.com/"), "2 hours 10 minutes ago", now_uSec - 130 * kUsecPerMin);
  addPlace(makeURI("http://4hour.com/"), "Less than 4 hours ago", now_uSec - 180 * kUsecPerMin);
  addPlace(makeURI("http://4hour10minutes.com/"), "4 hours 10 minutesago", now_uSec - 250 * kUsecPerMin);

  let today = new Date();
  today.setHours(0);
  today.setMinutes(0);
  today.setSeconds(1);
  addPlace(makeURI("http://today.com/"), "Today", today.getTime() * 1000);

  let lastYear = new Date();
  lastYear.setFullYear(lastYear.getFullYear() - 1);
  addPlace(makeURI("http://before-today.com/"), "Before Today", lastYear.getTime() * 1000);

  PlacesUtils.asyncHistory.updatePlaces(places, {
    handleError: function () ok(false, "Unexpected error in adding visit."),
    handleResult: function () { },
    handleCompletion: function () deferred.resolve()
  });

  return deferred.promise;
}

function setupFormHistory() {

  function searchEntries(terms, params) {
    let deferred = Promise.defer();

    let results = [];
    FormHistory.search(terms, params, { handleResult: function (result) results.push(result),
                                        handleError: function (error) {
                                          do_throw("Error occurred searching form history: " + error);
                                          deferred.reject(error);
                                        },
                                        handleCompletion: function (reason) { deferred.resolve(results); }
                                      });
    return deferred.promise;
  }

  function update(changes)
  {
    let deferred = Promise.defer();
    FormHistory.update(changes, { handleError: function (error) {
                                    do_throw("Error occurred searching form history: " + error);
                                    deferred.reject(error);
                                  },
                                  handleCompletion: function (reason) { deferred.resolve(); }
                                });
    return deferred.promise;
  }

  // Make sure we've got a clean DB to start with, then add the entries we'll be testing.
  yield update(
    [{
        op: "remove"
     },
     {
        op : "add",
        fieldname : "10minutes",
        value : "10m"
      }, {
        op : "add",
        fieldname : "1hour",
        value : "1h"
      }, {
        op : "add",
        fieldname : "1hour10minutes",
        value : "1h10m"
      }, {
        op : "add",
        fieldname : "2hour",
        value : "2h"
      }, {
        op : "add",
        fieldname : "2hour10minutes",
        value : "2h10m"
      }, {
        op : "add",
        fieldname : "4hour",
        value : "4h"
      }, {
        op : "add",
        fieldname : "4hour10minutes",
        value : "4h10m"
      }, {
        op : "add",
        fieldname : "today",
        value : "1d"
      }, {
        op : "add",
        fieldname : "b4today",
        value : "1y"
      }]);

  // Artifically age the entries to the proper vintage.
  let timestamp = now_uSec - 10 * kUsecPerMin;
  let results = yield searchEntries(["guid"], { fieldname: "10minutes" });
  yield update({ op: "update", firstUsed: timestamp, guid: results[0].guid });

  timestamp = now_uSec - 45 * kUsecPerMin;
  results = yield searchEntries(["guid"], { fieldname: "1hour" });
  yield update({ op: "update", firstUsed: timestamp, guid: results[0].guid });

  timestamp = now_uSec - 70 * kUsecPerMin;
  results = yield searchEntries(["guid"], { fieldname: "1hour10minutes" });
  yield update({ op: "update", firstUsed: timestamp, guid: results[0].guid });

  timestamp = now_uSec - 90 * kUsecPerMin;
  results = yield searchEntries(["guid"], { fieldname: "2hour" });
  yield update({ op: "update", firstUsed: timestamp, guid: results[0].guid });

  timestamp = now_uSec - 130 * kUsecPerMin;
  results = yield searchEntries(["guid"], { fieldname: "2hour10minutes" });
  yield update({ op: "update", firstUsed: timestamp, guid: results[0].guid });

  timestamp = now_uSec - 180 * kUsecPerMin;
  results = yield searchEntries(["guid"], { fieldname: "4hour" });
  yield update({ op: "update", firstUsed: timestamp, guid: results[0].guid });

  timestamp = now_uSec - 250 * kUsecPerMin;
  results = yield searchEntries(["guid"], { fieldname: "4hour10minutes" });
  yield update({ op: "update", firstUsed: timestamp, guid: results[0].guid });

  let today = new Date();
  today.setHours(0);
  today.setMinutes(0);
  today.setSeconds(1);
  timestamp = today.getTime() * 1000;
  results = yield searchEntries(["guid"], { fieldname: "today" });
  yield update({ op: "update", firstUsed: timestamp, guid: results[0].guid });

  let lastYear = new Date();
  lastYear.setFullYear(lastYear.getFullYear() - 1);
  timestamp = lastYear.getTime() * 1000;
  results = yield searchEntries(["guid"], { fieldname: "b4today" });
  yield update({ op: "update", firstUsed: timestamp, guid: results[0].guid });

  var checks = 0;
  let checkOne = function(num, message) { is(num, 1, message); checks++; }

  // Sanity check.
  yield countEntries("10minutes", "Checking for 10minutes form history entry creation", checkOne);
  yield countEntries("1hour", "Checking for 1hour form history entry creation", checkOne);
  yield countEntries("1hour10minutes", "Checking for 1hour10minutes form history entry creation", checkOne);
  yield countEntries("2hour", "Checking for 2hour form history entry creation", checkOne);
  yield countEntries("2hour10minutes", "Checking for 2hour10minutes form history entry creation", checkOne);
  yield countEntries("4hour", "Checking for 4hour form history entry creation", checkOne);
  yield countEntries("4hour10minutes", "Checking for 4hour10minutes form history entry creation", checkOne);
  yield countEntries("today", "Checking for today form history entry creation", checkOne);
  yield countEntries("b4today", "Checking for b4today form history entry creation", checkOne);
  is(checks, 9, "9 checks made");
}

function setupDownloads() {

  let publicList = yield Downloads.getPublicDownloadList();

  let download = yield Downloads.createDownload({
    source: "https://bugzilla.mozilla.org/show_bug.cgi?id=480169",
    target: "fakefile-10-minutes"
  });
  download.startTime = new Date(now_uSec - 10 * kUsecPerMin), // 10 minutes ago, in uSec
  download.canceled = true;
  publicList.add(download);

  download = yield Downloads.createDownload({
    source: "https://bugzilla.mozilla.org/show_bug.cgi?id=453440",
    target: "fakefile-1-hour"
  });
  download.startTime = new Date(now_uSec - 45 * kUsecPerMin), // 45 minutes ago, in uSec
  download.canceled = true;
  publicList.add(download);

  download = yield Downloads.createDownload({
    source: "https://bugzilla.mozilla.org/show_bug.cgi?id=480169",
    target: "fakefile-1-hour-10-minutes"
  });
  download.startTime = new Date(now_uSec - 70 * kUsecPerMin), // 70 minutes ago, in uSec
  download.canceled = true;
  publicList.add(download);

  download = yield Downloads.createDownload({
    source: "https://bugzilla.mozilla.org/show_bug.cgi?id=453440",
    target: "fakefile-2-hour"
  });
  download.startTime = new Date(now_uSec - 90 * kUsecPerMin), // 90 minutes ago, in uSec
  download.canceled = true;
  publicList.add(download);

  download = yield Downloads.createDownload({
    source: "https://bugzilla.mozilla.org/show_bug.cgi?id=480169",
    target: "fakefile-2-hour-10-minutes"
  });
  download.startTime = new Date(now_uSec - 130 * kUsecPerMin), // 130 minutes ago, in uSec
  download.canceled = true;
  publicList.add(download);

  download = yield Downloads.createDownload({
    source: "https://bugzilla.mozilla.org/show_bug.cgi?id=453440",
    target: "fakefile-4-hour"
  });
  download.startTime = new Date(now_uSec - 180 * kUsecPerMin), // 180 minutes ago, in uSec
  download.canceled = true;
  publicList.add(download);

  download = yield Downloads.createDownload({
    source: "https://bugzilla.mozilla.org/show_bug.cgi?id=480169",
    target: "fakefile-4-hour-10-minutes"
  });
  download.startTime = new Date(now_uSec - 250 * kUsecPerMin), // 250 minutes ago, in uSec
  download.canceled = true;
  publicList.add(download);

  // Add "today" download
  let today = new Date();
  today.setHours(0);
  today.setMinutes(0);
  today.setSeconds(1);

  download = yield Downloads.createDownload({
    source: "https://bugzilla.mozilla.org/show_bug.cgi?id=453440",
    target: "fakefile-today"
  });
  download.startTime = new Date(today.getTime() * 1000), // 12:00:30am this morning, in uSec
  download.canceled = true;
  publicList.add(download);
  
  // Add "before today" download
  let lastYear = new Date();
  lastYear.setFullYear(lastYear.getFullYear() - 1);

  download = yield Downloads.createDownload({
    source: "https://bugzilla.mozilla.org/show_bug.cgi?id=453440",
    target: "fakefile-old"
  });
  download.startTime = new Date(lastYear.getTime() * 1000), // 1 year ago, in uSec
  download.canceled = true;
  publicList.add(download);
  
  // Confirm everything worked
  let downloads = yield publicList.getAll();
  is(downloads.length, 9, "9 Pretend downloads added");

  ok((yield downloadExists(publicList, "fakefile-old")), "Pretend download for everything case should exist");
  ok((yield downloadExists(publicList, "fakefile-10-minutes")), "Pretend download for 10-minutes case should exist");
  ok((yield downloadExists(publicList, "fakefile-1-hour")), "Pretend download for 1-hour case should exist");
  ok((yield downloadExists(publicList, "fakefile-1-hour-10-minutes")), "Pretend download for 1-hour-10-minutes case should exist");
  ok((yield downloadExists(publicList, "fakefile-2-hour")), "Pretend download for 2-hour case should exist");
  ok((yield downloadExists(publicList, "fakefile-2-hour-10-minutes")), "Pretend download for 2-hour-10-minutes case should exist");
  ok((yield downloadExists(publicList, "fakefile-4-hour")), "Pretend download for 4-hour case should exist");
  ok((yield downloadExists(publicList, "fakefile-4-hour-10-minutes")), "Pretend download for 4-hour-10-minutes case should exist");
  ok((yield downloadExists(publicList, "fakefile-today")), "Pretend download for Today case should exist");
}

/**
 * Checks to see if the downloads with the specified id exists.
 *
 * @param aID
 *        The ids of the downloads to check.
 */
function downloadExists(list, path)
{
  return Task.spawn(function() {
    let listArray = yield list.getAll();
    throw new Task.Result(listArray.some(i => i.target.path == path));
  });
}

function isToday(aDate) {
  return aDate.getDate() == new Date().getDate();
}
