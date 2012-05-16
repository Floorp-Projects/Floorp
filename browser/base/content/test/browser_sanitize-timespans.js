// Bug 453440 - Test the timespan-based logic of the sanitizer code
var now_uSec = Date.now() * 1000;

const dm = Cc["@mozilla.org/download-manager;1"].getService(Ci.nsIDownloadManager);
const bhist = Cc["@mozilla.org/browser/global-history;2"].getService(Ci.nsIBrowserHistory);
const formhist = Cc["@mozilla.org/satchel/form-history;1"].getService(Ci.nsIFormHistory2);

const kUsecPerMin = 60 * 1000000;

let tempScope = {};
Cc["@mozilla.org/moz/jssubscript-loader;1"].getService(Ci.mozIJSSubScriptLoader)
                                           .loadSubScript("chrome://browser/content/sanitize.js", tempScope);
let Sanitizer = tempScope.Sanitizer;

function test() {
  waitForExplicitFinish();

  setupDownloads();
  setupFormHistory();
  setupHistory(onHistoryReady);
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

  // Clear 10 minutes ago
  s.range = [now_uSec - 10*60*1000000, now_uSec];
  s.sanitize();
  s.range = null;
  
  ok(!bhist.isVisited(makeURI("http://10minutes.com")), "10minutes.com should now be deleted");
  ok(bhist.isVisited(makeURI("http://1hour.com")), "Pretend visit to 1hour.com should still exist");
  ok(bhist.isVisited(makeURI("http://1hour10minutes.com/")), "Pretend visit to 1hour10minutes.com should still exist");
  ok(bhist.isVisited(makeURI("http://2hour.com")), "Pretend visit to 2hour.com should still exist");
  ok(bhist.isVisited(makeURI("http://2hour10minutes.com/")), "Pretend visit to 2hour10minutes.com should still exist");
  ok(bhist.isVisited(makeURI("http://4hour.com")), "Pretend visit to 4hour.com should still exist");
  ok(bhist.isVisited(makeURI("http://4hour10minutes.com/")), "Pretend visit to 4hour10minutes.com should still exist");
  
  if (minutesSinceMidnight > 10)
    ok(bhist.isVisited(makeURI("http://today.com")), "Pretend visit to today.com should still exist");
  ok(bhist.isVisited(makeURI("http://before-today.com")), "Pretend visit to before-today.com should still exist");
  
  ok(!formhist.nameExists("10minutes"), "10minutes form entry should be deleted");
  ok(formhist.nameExists("1hour"), "1hour form entry should still exist");
  ok(formhist.nameExists("1hour10minutes"), "1hour10minutes form entry should still exist");
  ok(formhist.nameExists("2hour"), "2hour form entry should still exist");
  ok(formhist.nameExists("2hour10minutes"), "2hour10minutes form entry should still exist");
  ok(formhist.nameExists("4hour"), "4hour form entry should still exist");
  ok(formhist.nameExists("4hour10minutes"), "4hour10minutes form entry should still exist");
  if (minutesSinceMidnight > 10)
    ok(formhist.nameExists("today"), "today form entry should still exist");
  ok(formhist.nameExists("b4today"), "b4today form entry should still exist");

  ok(!downloadExists(5555555), "10 minute download should now be deleted");
  ok(downloadExists(5555551), "<1 hour download should still be present");
  ok(downloadExists(5555556), "1 hour 10 minute download should still be present");
  ok(downloadExists(5555550), "Year old download should still be present");
  ok(downloadExists(5555552), "<2 hour old download should still be present");
  ok(downloadExists(5555557), "2 hour 10 minute download should still be present");
  ok(downloadExists(5555553), "<4 hour old download should still be present");
  ok(downloadExists(5555558), "4 hour 10 minute download should still be present");

  if (minutesSinceMidnight > 10)
    ok(downloadExists(5555554), "'Today' download should still be present");

  // Clear 1 hour
  Sanitizer.prefs.setIntPref("timeSpan", 1);
  s.sanitize();
  
  ok(!bhist.isVisited(makeURI("http://1hour.com")), "1hour.com should now be deleted");
  ok(bhist.isVisited(makeURI("http://1hour10minutes.com/")), "Pretend visit to 1hour10minutes.com should still exist");
  ok(bhist.isVisited(makeURI("http://2hour.com")), "Pretend visit to 2hour.com should still exist");
  ok(bhist.isVisited(makeURI("http://2hour10minutes.com/")), "Pretend visit to 2hour10minutes.com should still exist");
  ok(bhist.isVisited(makeURI("http://4hour.com")), "Pretend visit to 4hour.com should still exist");
  ok(bhist.isVisited(makeURI("http://4hour10minutes.com/")), "Pretend visit to 4hour10minutes.com should still exist");
  
  if (hoursSinceMidnight > 1)
    ok(bhist.isVisited(makeURI("http://today.com")), "Pretend visit to today.com should still exist");
  ok(bhist.isVisited(makeURI("http://before-today.com")), "Pretend visit to before-today.com should still exist");
  
  ok(!formhist.nameExists("1hour"), "1hour form entry should be deleted");
  ok(formhist.nameExists("1hour10minutes"), "1hour10minutes form entry should still exist");
  ok(formhist.nameExists("2hour"), "2hour form entry should still exist");
  ok(formhist.nameExists("2hour10minutes"), "2hour10minutes form entry should still exist");
  ok(formhist.nameExists("4hour"), "4hour form entry should still exist");
  ok(formhist.nameExists("4hour10minutes"), "4hour10minutes form entry should still exist");
  if (hoursSinceMidnight > 1)
    ok(formhist.nameExists("today"), "today form entry should still exist");
  ok(formhist.nameExists("b4today"), "b4today form entry should still exist");

  ok(!downloadExists(5555551), "<1 hour download should now be deleted");
  ok(downloadExists(5555556), "1 hour 10 minute download should still be present");
  ok(downloadExists(5555550), "Year old download should still be present");
  ok(downloadExists(5555552), "<2 hour old download should still be present");
  ok(downloadExists(5555557), "2 hour 10 minute download should still be present");
  ok(downloadExists(5555553), "<4 hour old download should still be present");
  ok(downloadExists(5555558), "4 hour 10 minute download should still be present");

  if (hoursSinceMidnight > 1)
    ok(downloadExists(5555554), "'Today' download should still be present");
  
  // Clear 1 hour 10 minutes
  s.range = [now_uSec - 70*60*1000000, now_uSec];
  s.sanitize();
  s.range = null;
  
  ok(!bhist.isVisited(makeURI("http://1hour10minutes.com")), "Pretend visit to 1hour10minutes.com should now be deleted");
  ok(bhist.isVisited(makeURI("http://2hour.com")), "Pretend visit to 2hour.com should still exist");
  ok(bhist.isVisited(makeURI("http://2hour10minutes.com/")), "Pretend visit to 2hour10minutes.com should still exist");
  ok(bhist.isVisited(makeURI("http://4hour.com")), "Pretend visit to 4hour.com should still exist");
  ok(bhist.isVisited(makeURI("http://4hour10minutes.com/")), "Pretend visit to 4hour10minutes.com should still exist");
  if (minutesSinceMidnight > 70)
    ok(bhist.isVisited(makeURI("http://today.com")), "Pretend visit to today.com should still exist");
  ok(bhist.isVisited(makeURI("http://before-today.com")), "Pretend visit to before-today.com should still exist");
  
  ok(!formhist.nameExists("1hour10minutes"), "1hour10minutes form entry should be deleted");
  ok(formhist.nameExists("2hour"), "2hour form entry should still exist");
  ok(formhist.nameExists("2hour10minutes"), "2hour10minutes form entry should still exist");
  ok(formhist.nameExists("4hour"), "4hour form entry should still exist");
  ok(formhist.nameExists("4hour10minutes"), "4hour10minutes form entry should still exist");
  if (minutesSinceMidnight > 70)
    ok(formhist.nameExists("today"), "today form entry should still exist");
  ok(formhist.nameExists("b4today"), "b4today form entry should still exist");

  ok(!downloadExists(5555556), "1 hour 10 minute old download should now be deleted");
  ok(downloadExists(5555550), "Year old download should still be present");
  ok(downloadExists(5555552), "<2 hour old download should still be present");
  ok(downloadExists(5555557), "2 hour 10 minute download should still be present");
  ok(downloadExists(5555553), "<4 hour old download should still be present");
  ok(downloadExists(5555558), "4 hour 10 minute download should still be present");
  if (minutesSinceMidnight > 70)
    ok(downloadExists(5555554), "'Today' download should still be present");

  // Clear 2 hours
  Sanitizer.prefs.setIntPref("timeSpan", 2);
  s.sanitize();
  
  ok(!bhist.isVisited(makeURI("http://2hour.com")), "Pretend visit to 2hour.com should now be deleted");
  ok(bhist.isVisited(makeURI("http://2hour10minutes.com/")), "Pretend visit to 2hour10minutes.com should still exist");
  ok(bhist.isVisited(makeURI("http://4hour.com")), "Pretend visit to 4hour.com should still exist");
  ok(bhist.isVisited(makeURI("http://4hour10minutes.com/")), "Pretend visit to 4hour10minutes.com should still exist");
  if (hoursSinceMidnight > 2)
    ok(bhist.isVisited(makeURI("http://today.com")), "Pretend visit to today.com should still exist");
  ok(bhist.isVisited(makeURI("http://before-today.com")), "Pretend visit to before-today.com should still exist");
  
  ok(!formhist.nameExists("2hour"), "2hour form entry should be deleted");
  ok(formhist.nameExists("2hour10minutes"), "2hour10minutes form entry should still exist");
  ok(formhist.nameExists("4hour"), "4hour form entry should still exist");
  ok(formhist.nameExists("4hour10minutes"), "4hour10minutes form entry should still exist");
  if (hoursSinceMidnight > 2)
    ok(formhist.nameExists("today"), "today form entry should still exist");
  ok(formhist.nameExists("b4today"), "b4today form entry should still exist");

  ok(formhist.nameExists("b4today"), "b4today form entry should still exist");
  ok(!downloadExists(5555552), "<2 hour old download should now be deleted");
  ok(downloadExists(5555550), "Year old download should still be present");
  ok(downloadExists(5555557), "2 hour 10 minute download should still be present");
  ok(downloadExists(5555553), "<4 hour old download should still be present");
  ok(downloadExists(5555558), "4 hour 10 minute download should still be present");
  if (hoursSinceMidnight > 2)
    ok(downloadExists(5555554), "'Today' download should still be present");
  
  // Clear 2 hours 10 minutes
  s.range = [now_uSec - 130*60*1000000, now_uSec];
  s.sanitize();
  s.range = null;
  
  ok(!bhist.isVisited(makeURI("http://2hour10minutes.com")), "Pretend visit to 2hour10minutes.com should now be deleted");
  ok(bhist.isVisited(makeURI("http://4hour.com")), "Pretend visit to 4hour.com should still exist");
  ok(bhist.isVisited(makeURI("http://4hour10minutes.com/")), "Pretend visit to 4hour10minutes.com should still exist");
  if (minutesSinceMidnight > 130)
    ok(bhist.isVisited(makeURI("http://today.com")), "Pretend visit to today.com should still exist");
  ok(bhist.isVisited(makeURI("http://before-today.com")), "Pretend visit to before-today.com should still exist");
  
  ok(!formhist.nameExists("2hour10minutes"), "2hour10minutes form entry should be deleted");
  ok(formhist.nameExists("4hour"), "4hour form entry should still exist");
  ok(formhist.nameExists("4hour10minutes"), "4hour10minutes form entry should still exist");
  if (minutesSinceMidnight > 130)
    ok(formhist.nameExists("today"), "today form entry should still exist");
  ok(formhist.nameExists("b4today"), "b4today form entry should still exist");

  ok(!downloadExists(5555557), "2 hour 10 minute old download should now be deleted");
  ok(downloadExists(5555553), "<4 hour old download should still be present");
  ok(downloadExists(5555558), "4 hour 10 minute download should still be present");
  ok(downloadExists(5555550), "Year old download should still be present");
  if (minutesSinceMidnight > 130)
    ok(downloadExists(5555554), "'Today' download should still be present");

  // Clear 4 hours
  Sanitizer.prefs.setIntPref("timeSpan", 3);
  s.sanitize();
  
  ok(!bhist.isVisited(makeURI("http://4hour.com")), "Pretend visit to 4hour.com should now be deleted");
  ok(bhist.isVisited(makeURI("http://4hour10minutes.com/")), "Pretend visit to 4hour10minutes.com should still exist");
  if (hoursSinceMidnight > 4)
    ok(bhist.isVisited(makeURI("http://today.com")), "Pretend visit to today.com should still exist");
  ok(bhist.isVisited(makeURI("http://before-today.com")), "Pretend visit to before-today.com should still exist");
  
  ok(!formhist.nameExists("4hour"), "4hour form entry should be deleted");
  ok(formhist.nameExists("4hour10minutes"), "4hour10minutes form entry should still exist");
  if (hoursSinceMidnight > 4)
    ok(formhist.nameExists("today"), "today form entry should still exist");
  ok(formhist.nameExists("b4today"), "b4today form entry should still exist");

  ok(!downloadExists(5555553), "<4 hour old download should now be deleted");
  ok(downloadExists(5555558), "4 hour 10 minute download should still be present");
  ok(downloadExists(5555550), "Year old download should still be present");
  if (hoursSinceMidnight > 4)
    ok(downloadExists(5555554), "'Today' download should still be present");

  // Clear 4 hours 10 minutes
  s.range = [now_uSec - 250*60*1000000, now_uSec];
  s.sanitize();
  s.range = null;
  
  ok(!bhist.isVisited(makeURI("http://4hour10minutes.com/")), "Pretend visit to 4hour10minutes.com should now be deleted");
  if (minutesSinceMidnight > 250)
    ok(bhist.isVisited(makeURI("http://today.com")), "Pretend visit to today.com should still exist");
  ok(bhist.isVisited(makeURI("http://before-today.com")), "Pretend visit to before-today.com should still exist");
  
  ok(!formhist.nameExists("4hour10minutes"), "4hour10minutes form entry should be deleted");
  if (minutesSinceMidnight > 250)
    ok(formhist.nameExists("today"), "today form entry should still exist");
  ok(formhist.nameExists("b4today"), "b4today form entry should still exist");

  ok(!downloadExists(5555558), "4 hour 10 minute download should now be deleted");
  ok(downloadExists(5555550), "Year old download should still be present");
  if (minutesSinceMidnight > 250)
    ok(downloadExists(5555554), "'Today' download should still be present");

  // Clear Today
  Sanitizer.prefs.setIntPref("timeSpan", 4);
  s.sanitize();

  // Be careful.  If we add our objectss just before midnight, and sanitize
  // runs immediately after, they won't be expired.  This is expected, but
  // we should not test in that case.  We cannot just test for opposite
  // condition because we could cross midnight just one moment after we
  // cache our time, then we would have an even worse random failure.
  var today = isToday(new Date(now_uSec/1000));
  if (today) {
    ok(!bhist.isVisited(makeURI("http://today.com")), "Pretend visit to today.com should now be deleted");
    ok(!formhist.nameExists("today"), "today form entry should be deleted");
    ok(!downloadExists(5555554), "'Today' download should now be deleted");
  }

  ok(bhist.isVisited(makeURI("http://before-today.com")), "Pretend visit to before-today.com should still exist");
  ok(formhist.nameExists("b4today"), "b4today form entry should still exist");
  ok(downloadExists(5555550), "Year old download should still be present");

  // Choose everything
  Sanitizer.prefs.setIntPref("timeSpan", 0);
  s.sanitize();

  ok(!bhist.isVisited(makeURI("http://before-today.com")), "Pretend visit to before-today.com should now be deleted");

  ok(!formhist.nameExists("b4today"), "b4today form entry should be deleted");

  ok(!downloadExists(5555550), "Year old download should now be deleted");

  finish();
}

function setupHistory(aCallback) {
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
    handleCompletion: function () aCallback()
  });
}

function setupFormHistory() {
  // Make sure we've got a clean DB to start with.
  formhist.removeAllEntries();

  // Add the entries we'll be testing.
  formhist.addEntry("10minutes", "10m");
  formhist.addEntry("1hour", "1h");
  formhist.addEntry("1hour10minutes", "1h10m");
  formhist.addEntry("2hour", "2h");
  formhist.addEntry("2hour10minutes", "2h10m");
  formhist.addEntry("4hour", "4h");
  formhist.addEntry("4hour10minutes", "4h10m");
  formhist.addEntry("today", "1d");
  formhist.addEntry("b4today", "1y");

  // Artifically age the entries to the proper vintage.
  let db = formhist.DBConnection;
  let timestamp = now_uSec - 10 * kUsecPerMin;
  db.executeSimpleSQL("UPDATE moz_formhistory SET firstUsed = " +
                      timestamp +  " WHERE fieldname = '10minutes'");
  timestamp = now_uSec - 45 * kUsecPerMin;
  db.executeSimpleSQL("UPDATE moz_formhistory SET firstUsed = " +
                      timestamp +  " WHERE fieldname = '1hour'");
  timestamp = now_uSec - 70 * kUsecPerMin;
  db.executeSimpleSQL("UPDATE moz_formhistory SET firstUsed = " +
                      timestamp +  " WHERE fieldname = '1hour10minutes'");
  timestamp = now_uSec - 90 * kUsecPerMin;
  db.executeSimpleSQL("UPDATE moz_formhistory SET firstUsed = " +
                      timestamp +  " WHERE fieldname = '2hour'");
  timestamp = now_uSec - 130 * kUsecPerMin;
  db.executeSimpleSQL("UPDATE moz_formhistory SET firstUsed = " +
                      timestamp +  " WHERE fieldname = '2hour10minutes'");
  timestamp = now_uSec - 180 * kUsecPerMin;
  db.executeSimpleSQL("UPDATE moz_formhistory SET firstUsed = " +
                      timestamp +  " WHERE fieldname = '4hour'");
  timestamp = now_uSec - 250 * kUsecPerMin;
  db.executeSimpleSQL("UPDATE moz_formhistory SET firstUsed = " +
                      timestamp +  " WHERE fieldname = '4hour10minutes'");

  let today = new Date();
  today.setHours(0);
  today.setMinutes(0);
  today.setSeconds(1);
  timestamp = today.getTime() * 1000;
  db.executeSimpleSQL("UPDATE moz_formhistory SET firstUsed = " +
                      timestamp +  " WHERE fieldname = 'today'");

  let lastYear = new Date();
  lastYear.setFullYear(lastYear.getFullYear() - 1);
  timestamp = lastYear.getTime() * 1000;
  db.executeSimpleSQL("UPDATE moz_formhistory SET firstUsed = " +
                      timestamp +  " WHERE fieldname = 'b4today'");

  // Sanity check.
  ok(formhist.nameExists("10minutes"), "Checking for 10minutes form history entry creation");
  ok(formhist.nameExists("1hour"), "Checking for 1hour form history entry creation");
  ok(formhist.nameExists("1hour10minutes"), "Checking for 1hour10minutes form history entry creation");
  ok(formhist.nameExists("2hour"), "Checking for 2hour form history entry creation");
  ok(formhist.nameExists("2hour10minutes"), "Checking for 2hour10minutes form history entry creation");
  ok(formhist.nameExists("4hour"), "Checking for 4hour form history entry creation");
  ok(formhist.nameExists("4hour10minutes"), "Checking for 4hour10minutes form history entry creation");
  ok(formhist.nameExists("today"), "Checking for today form history entry creation");
  ok(formhist.nameExists("b4today"), "Checking for b4today form history entry creation");
}

function setupDownloads() {

  // Add 10-minutes download to DB
  let data = {
    id:   "5555555",
    name: "fakefile-10-minutes",
    source: "https://bugzilla.mozilla.org/show_bug.cgi?id=480169",
    target: "fakefile-10-minutes",
    startTime: now_uSec - 10 * kUsecPerMin, // 10 minutes ago, in uSec
    endTime: now_uSec - 11 * kUsecPerMin, // 1 minute later
    state: Ci.nsIDownloadManager.DOWNLOAD_FINISHED,
    currBytes: 0, maxBytes: -1, preferredAction: 0, autoResume: 0
  };

  let db = dm.DBConnection;
  let stmt = db.createStatement(
    "INSERT INTO moz_downloads (id, name, source, target, startTime, endTime, " +
      "state, currBytes, maxBytes, preferredAction, autoResume) " +
    "VALUES (:id, :name, :source, :target, :startTime, :endTime, :state, " +
      ":currBytes, :maxBytes, :preferredAction, :autoResume)");
  try {
    for (let prop in data)
      stmt.params[prop] = data[prop];
    stmt.execute();
  }
  finally {
    stmt.reset();
  }
  
  // Add within-1-hour download to DB
  data = {
    id:   "5555551",
    name: "fakefile-1-hour",
    source: "https://bugzilla.mozilla.org/show_bug.cgi?id=453440",
    target: "fakefile-1-hour",
    startTime: now_uSec - 45 * kUsecPerMin, // 45 minutes ago, in uSec
    endTime: now_uSec - 44 * kUsecPerMin, // 1 minute later
    state: Ci.nsIDownloadManager.DOWNLOAD_FINISHED,
    currBytes: 0, maxBytes: -1, preferredAction: 0, autoResume: 0
  };

  try {
    for (let prop in data)
      stmt.params[prop] = data[prop];
    stmt.execute();
  }
  finally {
    stmt.reset();
  }
  
  // Add 1-hour-10-minutes download to DB
  data = {
    id:   "5555556",
    name: "fakefile-1-hour-10-minutes",
    source: "https://bugzilla.mozilla.org/show_bug.cgi?id=480169",
    target: "fakefile-1-hour-10-minutes",
    startTime: now_uSec - 70 * kUsecPerMin, // 70 minutes ago, in uSec
    endTime: now_uSec - 71 * kUsecPerMin, // 1 minute later
    state: Ci.nsIDownloadManager.DOWNLOAD_FINISHED,
    currBytes: 0, maxBytes: -1, preferredAction: 0, autoResume: 0
  };

  try {
    for (let prop in data)
      stmt.params[prop] = data[prop];
    stmt.execute();
  }
  finally {
    stmt.reset();
  }

  // Add within-2-hour download  
  data = {
    id:   "5555552",
    name: "fakefile-2-hour",
    source: "https://bugzilla.mozilla.org/show_bug.cgi?id=453440",
    target: "fakefile-2-hour",
    startTime: now_uSec - 90 * kUsecPerMin, // 90 minutes ago, in uSec
    endTime: now_uSec - 89 * kUsecPerMin, // 1 minute later
    state: Ci.nsIDownloadManager.DOWNLOAD_FINISHED,
    currBytes: 0, maxBytes: -1, preferredAction: 0, autoResume: 0
  };

  try {
    for (let prop in data)
      stmt.params[prop] = data[prop];
    stmt.execute();
  }
  finally {
    stmt.reset();
  }
  
  // Add 2-hour-10-minutes download  
  data = {
    id:   "5555557",
    name: "fakefile-2-hour-10-minutes",
    source: "https://bugzilla.mozilla.org/show_bug.cgi?id=480169",
    target: "fakefile-2-hour-10-minutes",
    startTime: now_uSec - 130 * kUsecPerMin, // 130 minutes ago, in uSec
    endTime: now_uSec - 131 * kUsecPerMin, // 1 minute later
    state: Ci.nsIDownloadManager.DOWNLOAD_FINISHED,
    currBytes: 0, maxBytes: -1, preferredAction: 0, autoResume: 0
  };

  try {
    for (let prop in data)
      stmt.params[prop] = data[prop];
    stmt.execute();
  }
  finally {
    stmt.reset();
  }

  // Add within-4-hour download
  data = {
    id:   "5555553",
    name: "fakefile-4-hour",
    source: "https://bugzilla.mozilla.org/show_bug.cgi?id=453440",
    target: "fakefile-4-hour",
    startTime: now_uSec - 180 * kUsecPerMin, // 180 minutes ago, in uSec
    endTime: now_uSec - 179 * kUsecPerMin, // 1 minute later
    state: Ci.nsIDownloadManager.DOWNLOAD_FINISHED,
    currBytes: 0, maxBytes: -1, preferredAction: 0, autoResume: 0
  };
  
  try {
    for (let prop in data)
      stmt.params[prop] = data[prop];
    stmt.execute();
  }
  finally {
    stmt.reset();
  }
  
  // Add 4-hour-10-minutes download
  data = {
    id:   "5555558",
    name: "fakefile-4-hour-10-minutes",
    source: "https://bugzilla.mozilla.org/show_bug.cgi?id=480169",
    target: "fakefile-4-hour-10-minutes",
    startTime: now_uSec - 250 * kUsecPerMin, // 250 minutes ago, in uSec
    endTime: now_uSec - 251 * kUsecPerMin, // 1 minute later
    state: Ci.nsIDownloadManager.DOWNLOAD_FINISHED,
    currBytes: 0, maxBytes: -1, preferredAction: 0, autoResume: 0
  };
  
  try {
    for (let prop in data)
      stmt.params[prop] = data[prop];
    stmt.execute();
  }
  finally {
    stmt.reset();
  }

  // Add "today" download
  let today = new Date();
  today.setHours(0);
  today.setMinutes(0);
  today.setSeconds(1);
  
  data = {
    id:   "5555554",
    name: "fakefile-today",
    source: "https://bugzilla.mozilla.org/show_bug.cgi?id=453440",
    target: "fakefile-today",
    startTime: today.getTime() * 1000,  // 12:00:30am this morning, in uSec
    endTime: (today.getTime() + 1000) * 1000, // 1 second later
    state: Ci.nsIDownloadManager.DOWNLOAD_FINISHED,
    currBytes: 0, maxBytes: -1, preferredAction: 0, autoResume: 0
  };
  
  try {
    for (let prop in data)
      stmt.params[prop] = data[prop];
    stmt.execute();
  }
  finally {
    stmt.reset();
  }
  
  // Add "before today" download
  let lastYear = new Date();
  lastYear.setFullYear(lastYear.getFullYear() - 1);
  data = {
    id:   "5555550",
    name: "fakefile-old",
    source: "https://bugzilla.mozilla.org/show_bug.cgi?id=453440",
    target: "fakefile-old",
    startTime: lastYear.getTime() * 1000, // 1 year ago, in uSec
    endTime: (lastYear.getTime() + 1000) * 1000, // 1 second later
    state: Ci.nsIDownloadManager.DOWNLOAD_FINISHED,
    currBytes: 0, maxBytes: -1, preferredAction: 0, autoResume: 0
  };
  
  try {
    for (let prop in data)
      stmt.params[prop] = data[prop];
    stmt.execute();
  }
  finally {
    stmt.finalize();
  }
  
  // Confirm everything worked
  ok(downloadExists(5555550), "Pretend download for everything case should exist");
  ok(downloadExists(5555555), "Pretend download for 10-minutes case should exist");
  ok(downloadExists(5555551), "Pretend download for 1-hour case should exist");
  ok(downloadExists(5555556), "Pretend download for 1-hour-10-minutes case should exist");
  ok(downloadExists(5555552), "Pretend download for 2-hour case should exist");
  ok(downloadExists(5555557), "Pretend download for 2-hour-10-minutes case should exist");
  ok(downloadExists(5555553), "Pretend download for 4-hour case should exist");
  ok(downloadExists(5555558), "Pretend download for 4-hour-10-minutes case should exist");
  ok(downloadExists(5555554), "Pretend download for Today case should exist");
}

/**
 * Checks to see if the downloads with the specified id exists.
 *
 * @param aID
 *        The ids of the downloads to check.
 */
function downloadExists(aID)
{
  let db = dm.DBConnection;
  let stmt = db.createStatement(
    "SELECT * " +
    "FROM moz_downloads " +
    "WHERE id = :id"
  );
  stmt.params.id = aID;
  var rows = stmt.executeStep();
  stmt.finalize();
  return rows;
}

function isToday(aDate) {
  return aDate.getDate() == new Date().getDate();
}
