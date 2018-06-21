requestLongerTimeout(2);

const {PlacesTestUtils} =
  ChromeUtils.import("resource://testing-common/PlacesTestUtils.jsm", {});

// Bug 453440 - Test the timespan-based logic of the sanitizer code
var now_mSec = Date.now();
var now_uSec = now_mSec * 1000;

const kMsecPerMin = 60 * 1000;
const kUsecPerMin = 60 * 1000000;

function promiseFormHistoryRemoved() {
  return new Promise(resolve => {
    Services.obs.addObserver(function onfh() {
      Services.obs.removeObserver(onfh, "satchel-storage-changed");
      resolve();
    }, "satchel-storage-changed");
  });
}

function promiseDownloadRemoved(list) {
  return new Promise(resolve => {

    let view = {
      onDownloadRemoved(download) {
        list.removeView(view);
        resolve();
      }
    };

    list.addView(view);

  });
}

add_task(async function test() {
  await setupDownloads();
  await setupFormHistory();
  await setupHistory();
  await onHistoryReady();
});

function countEntries(name, message, check) {
  return new Promise((resolve, reject) => {

    var obj = {};
    if (name !== null)
      obj.fieldname = name;

    let count;
    FormHistory.count(obj, { handleResult: result => count = result,
                             handleError(error) {
                               reject(error);
                               throw new Error("Error occurred searching form history: " + error);
                             },
                             handleCompletion(reason) {
                               if (!reason) {
                                 check(count, message);
                                 resolve();
                               }
                             },
                           });

  });
}

async function onHistoryReady() {
  var hoursSinceMidnight = new Date().getHours();
  var minutesSinceMidnight = hoursSinceMidnight * 60 + new Date().getMinutes();

  // Should test cookies here, but nsICookieManager/nsICookieService
  // doesn't let us fake creation times.  bug 463127

  var itemPrefs = Services.prefs.getBranch("privacy.cpd.");
  itemPrefs.setBoolPref("history", true);
  itemPrefs.setBoolPref("downloads", true);
  itemPrefs.setBoolPref("cache", false);
  itemPrefs.setBoolPref("cookies", false);
  itemPrefs.setBoolPref("formdata", true);
  itemPrefs.setBoolPref("offlineApps", false);
  itemPrefs.setBoolPref("passwords", false);
  itemPrefs.setBoolPref("sessions", false);
  itemPrefs.setBoolPref("siteSettings", false);

  let publicList = await Downloads.getList(Downloads.PUBLIC);
  let downloadPromise = promiseDownloadRemoved(publicList);
  let formHistoryPromise = promiseFormHistoryRemoved();

  // Clear 10 minutes ago
  let range = [now_uSec - 10 * 60 * 1000000, now_uSec];
  await Sanitizer.sanitize(null, {range, ignoreTimespan: false});

  await formHistoryPromise;
  await downloadPromise;

  ok(!(await PlacesUtils.history.hasVisits("http://10minutes.com")),
     "Pretend visit to 10minutes.com should now be deleted");
  ok((await PlacesUtils.history.hasVisits("http://1hour.com")),
     "Pretend visit to 1hour.com should should still exist");
  ok((await PlacesUtils.history.hasVisits("http://1hour10minutes.com")),
     "Pretend visit to 1hour10minutes.com should should still exist");
  ok((await PlacesUtils.history.hasVisits("http://2hour.com")),
     "Pretend visit to 2hour.com should should still exist");
  ok((await PlacesUtils.history.hasVisits("http://2hour10minutes.com")),
     "Pretend visit to 2hour10minutes.com should should still exist");
  ok((await PlacesUtils.history.hasVisits("http://4hour.com")),
     "Pretend visit to 4hour.com should should still exist");
  ok((await PlacesUtils.history.hasVisits("http://4hour10minutes.com")),
     "Pretend visit to 4hour10minutes.com should should still exist");
  if (minutesSinceMidnight > 10) {
    ok((await PlacesUtils.history.hasVisits("http://today.com")),
       "Pretend visit to today.com should still exist");
  }
  ok((await PlacesUtils.history.hasVisits("http://before-today.com")),
    "Pretend visit to before-today.com should still exist");

  let checkZero = function(num, message) { is(num, 0, message); };
  let checkOne = function(num, message) { is(num, 1, message); };

  await countEntries("10minutes", "10minutes form entry should be deleted", checkZero);
  await countEntries("1hour", "1hour form entry should still exist", checkOne);
  await countEntries("1hour10minutes", "1hour10minutes form entry should still exist", checkOne);
  await countEntries("2hour", "2hour form entry should still exist", checkOne);
  await countEntries("2hour10minutes", "2hour10minutes form entry should still exist", checkOne);
  await countEntries("4hour", "4hour form entry should still exist", checkOne);
  await countEntries("4hour10minutes", "4hour10minutes form entry should still exist", checkOne);
  if (minutesSinceMidnight > 10)
    await countEntries("today", "today form entry should still exist", checkOne);
  await countEntries("b4today", "b4today form entry should still exist", checkOne);

  ok(!(await downloadExists(publicList, "fakefile-10-minutes")), "10 minute download should now be deleted");
  ok((await downloadExists(publicList, "fakefile-1-hour")), "<1 hour download should still be present");
  ok((await downloadExists(publicList, "fakefile-1-hour-10-minutes")), "1 hour 10 minute download should still be present");
  ok((await downloadExists(publicList, "fakefile-old")), "Year old download should still be present");
  ok((await downloadExists(publicList, "fakefile-2-hour")), "<2 hour old download should still be present");
  ok((await downloadExists(publicList, "fakefile-2-hour-10-minutes")), "2 hour 10 minute download should still be present");
  ok((await downloadExists(publicList, "fakefile-4-hour")), "<4 hour old download should still be present");
  ok((await downloadExists(publicList, "fakefile-4-hour-10-minutes")), "4 hour 10 minute download should still be present");

  if (minutesSinceMidnight > 10)
    ok((await downloadExists(publicList, "fakefile-today")), "'Today' download should still be present");

  downloadPromise = promiseDownloadRemoved(publicList);
  formHistoryPromise = promiseFormHistoryRemoved();

  // Clear 1 hour
  Services.prefs.setIntPref(Sanitizer.PREF_TIMESPAN, 1);
  await Sanitizer.sanitize(null, {ignoreTimespan: false});

  await formHistoryPromise;
  await downloadPromise;

  ok(!(await PlacesUtils.history.hasVisits("http://1hour.com")),
     "Pretend visit to 1hour.com should now be deleted");
  ok((await PlacesUtils.history.hasVisits("http://1hour10minutes.com")),
     "Pretend visit to 1hour10minutes.com should should still exist");
  ok((await PlacesUtils.history.hasVisits("http://2hour.com")),
     "Pretend visit to 2hour.com should should still exist");
  ok((await PlacesUtils.history.hasVisits("http://2hour10minutes.com")),
     "Pretend visit to 2hour10minutes.com should should still exist");
  ok((await PlacesUtils.history.hasVisits("http://4hour.com")),
     "Pretend visit to 4hour.com should should still exist");
  ok((await PlacesUtils.history.hasVisits("http://4hour10minutes.com")),
     "Pretend visit to 4hour10minutes.com should should still exist");
  if (hoursSinceMidnight > 1) {
    ok((await PlacesUtils.history.hasVisits("http://today.com")),
       "Pretend visit to today.com should still exist");
  }
  ok((await PlacesUtils.history.hasVisits("http://before-today.com")),
    "Pretend visit to before-today.com should still exist");

  await countEntries("1hour", "1hour form entry should be deleted", checkZero);
  await countEntries("1hour10minutes", "1hour10minutes form entry should still exist", checkOne);
  await countEntries("2hour", "2hour form entry should still exist", checkOne);
  await countEntries("2hour10minutes", "2hour10minutes form entry should still exist", checkOne);
  await countEntries("4hour", "4hour form entry should still exist", checkOne);
  await countEntries("4hour10minutes", "4hour10minutes form entry should still exist", checkOne);
  if (hoursSinceMidnight > 1)
    await countEntries("today", "today form entry should still exist", checkOne);
  await countEntries("b4today", "b4today form entry should still exist", checkOne);

  ok(!(await downloadExists(publicList, "fakefile-1-hour")), "<1 hour download should now be deleted");
  ok((await downloadExists(publicList, "fakefile-1-hour-10-minutes")), "1 hour 10 minute download should still be present");
  ok((await downloadExists(publicList, "fakefile-old")), "Year old download should still be present");
  ok((await downloadExists(publicList, "fakefile-2-hour")), "<2 hour old download should still be present");
  ok((await downloadExists(publicList, "fakefile-2-hour-10-minutes")), "2 hour 10 minute download should still be present");
  ok((await downloadExists(publicList, "fakefile-4-hour")), "<4 hour old download should still be present");
  ok((await downloadExists(publicList, "fakefile-4-hour-10-minutes")), "4 hour 10 minute download should still be present");

  if (hoursSinceMidnight > 1)
    ok((await downloadExists(publicList, "fakefile-today")), "'Today' download should still be present");

  downloadPromise = promiseDownloadRemoved(publicList);
  formHistoryPromise = promiseFormHistoryRemoved();

  // Clear 1 hour 10 minutes
  range = [now_uSec - 70 * 60 * 1000000, now_uSec];
  await Sanitizer.sanitize(null, {range, ignoreTimespan: false});

  await formHistoryPromise;
  await downloadPromise;

  ok(!(await PlacesUtils.history.hasVisits("http://1hour10minutes.com")),
     "Pretend visit to 1hour10minutes.com should now be deleted");
  ok((await PlacesUtils.history.hasVisits("http://2hour.com")),
     "Pretend visit to 2hour.com should should still exist");
  ok((await PlacesUtils.history.hasVisits("http://2hour10minutes.com")),
     "Pretend visit to 2hour10minutes.com should should still exist");
  ok((await PlacesUtils.history.hasVisits("http://4hour.com")),
     "Pretend visit to 4hour.com should should still exist");
  ok((await PlacesUtils.history.hasVisits("http://4hour10minutes.com")),
     "Pretend visit to 4hour10minutes.com should should still exist");
  if (minutesSinceMidnight > 70) {
    ok((await PlacesUtils.history.hasVisits("http://today.com")),
       "Pretend visit to today.com should still exist");
  }
  ok((await PlacesUtils.history.hasVisits("http://before-today.com")),
    "Pretend visit to before-today.com should still exist");

  await countEntries("1hour10minutes", "1hour10minutes form entry should be deleted", checkZero);
  await countEntries("2hour", "2hour form entry should still exist", checkOne);
  await countEntries("2hour10minutes", "2hour10minutes form entry should still exist", checkOne);
  await countEntries("4hour", "4hour form entry should still exist", checkOne);
  await countEntries("4hour10minutes", "4hour10minutes form entry should still exist", checkOne);
  if (minutesSinceMidnight > 70)
    await countEntries("today", "today form entry should still exist", checkOne);
  await countEntries("b4today", "b4today form entry should still exist", checkOne);

  ok(!(await downloadExists(publicList, "fakefile-1-hour-10-minutes")), "1 hour 10 minute old download should now be deleted");
  ok((await downloadExists(publicList, "fakefile-old")), "Year old download should still be present");
  ok((await downloadExists(publicList, "fakefile-2-hour")), "<2 hour old download should still be present");
  ok((await downloadExists(publicList, "fakefile-2-hour-10-minutes")), "2 hour 10 minute download should still be present");
  ok((await downloadExists(publicList, "fakefile-4-hour")), "<4 hour old download should still be present");
  ok((await downloadExists(publicList, "fakefile-4-hour-10-minutes")), "4 hour 10 minute download should still be present");
  if (minutesSinceMidnight > 70)
    ok((await downloadExists(publicList, "fakefile-today")), "'Today' download should still be present");

  downloadPromise = promiseDownloadRemoved(publicList);
  formHistoryPromise = promiseFormHistoryRemoved();

  // Clear 2 hours
  Services.prefs.setIntPref(Sanitizer.PREF_TIMESPAN, 2);
  await Sanitizer.sanitize(null, {ignoreTimespan: false});

  await formHistoryPromise;
  await downloadPromise;

  ok(!(await PlacesUtils.history.hasVisits("http://2hour.com")),
     "Pretend visit to 2hour.com should now be deleted");
  ok((await PlacesUtils.history.hasVisits("http://2hour10minutes.com")),
     "Pretend visit to 2hour10minutes.com should should still exist");
  ok((await PlacesUtils.history.hasVisits("http://4hour.com")),
     "Pretend visit to 4hour.com should should still exist");
  ok((await PlacesUtils.history.hasVisits("http://4hour10minutes.com")),
     "Pretend visit to 4hour10minutes.com should should still exist");
  if (hoursSinceMidnight > 2) {
    ok((await PlacesUtils.history.hasVisits("http://today.com")),
       "Pretend visit to today.com should still exist");
  }
  ok((await PlacesUtils.history.hasVisits("http://before-today.com")),
    "Pretend visit to before-today.com should still exist");

  await countEntries("2hour", "2hour form entry should be deleted", checkZero);
  await countEntries("2hour10minutes", "2hour10minutes form entry should still exist", checkOne);
  await countEntries("4hour", "4hour form entry should still exist", checkOne);
  await countEntries("4hour10minutes", "4hour10minutes form entry should still exist", checkOne);
  if (hoursSinceMidnight > 2)
    await countEntries("today", "today form entry should still exist", checkOne);
  await countEntries("b4today", "b4today form entry should still exist", checkOne);

  ok(!(await downloadExists(publicList, "fakefile-2-hour")), "<2 hour old download should now be deleted");
  ok((await downloadExists(publicList, "fakefile-old")), "Year old download should still be present");
  ok((await downloadExists(publicList, "fakefile-2-hour-10-minutes")), "2 hour 10 minute download should still be present");
  ok((await downloadExists(publicList, "fakefile-4-hour")), "<4 hour old download should still be present");
  ok((await downloadExists(publicList, "fakefile-4-hour-10-minutes")), "4 hour 10 minute download should still be present");
  if (hoursSinceMidnight > 2)
    ok((await downloadExists(publicList, "fakefile-today")), "'Today' download should still be present");

  downloadPromise = promiseDownloadRemoved(publicList);
  formHistoryPromise = promiseFormHistoryRemoved();

  // Clear 2 hours 10 minutes
  range = [now_uSec - 130 * 60 * 1000000, now_uSec];
  await Sanitizer.sanitize(null, {range, ignoreTimespan: false});

  await formHistoryPromise;
  await downloadPromise;

  ok(!(await PlacesUtils.history.hasVisits("http://2hour10minutes.com")),
     "Pretend visit to 2hour10minutes.com should now be deleted");
  ok((await PlacesUtils.history.hasVisits("http://4hour.com")),
     "Pretend visit to 4hour.com should should still exist");
  ok((await PlacesUtils.history.hasVisits("http://4hour10minutes.com")),
     "Pretend visit to 4hour10minutes.com should should still exist");
  if (minutesSinceMidnight > 130) {
    ok((await PlacesUtils.history.hasVisits("http://today.com")),
       "Pretend visit to today.com should still exist");
  }
  ok((await PlacesUtils.history.hasVisits("http://before-today.com")),
    "Pretend visit to before-today.com should still exist");

  await countEntries("2hour10minutes", "2hour10minutes form entry should be deleted", checkZero);
  await countEntries("4hour", "4hour form entry should still exist", checkOne);
  await countEntries("4hour10minutes", "4hour10minutes form entry should still exist", checkOne);
  if (minutesSinceMidnight > 130)
    await countEntries("today", "today form entry should still exist", checkOne);
  await countEntries("b4today", "b4today form entry should still exist", checkOne);

  ok(!(await downloadExists(publicList, "fakefile-2-hour-10-minutes")), "2 hour 10 minute old download should now be deleted");
  ok((await downloadExists(publicList, "fakefile-4-hour")), "<4 hour old download should still be present");
  ok((await downloadExists(publicList, "fakefile-4-hour-10-minutes")), "4 hour 10 minute download should still be present");
  ok((await downloadExists(publicList, "fakefile-old")), "Year old download should still be present");
  if (minutesSinceMidnight > 130)
    ok((await downloadExists(publicList, "fakefile-today")), "'Today' download should still be present");

  downloadPromise = promiseDownloadRemoved(publicList);
  formHistoryPromise = promiseFormHistoryRemoved();

  // Clear 4 hours
  Services.prefs.setIntPref(Sanitizer.PREF_TIMESPAN, 3);
  await Sanitizer.sanitize(null, {ignoreTimespan: false});

  await formHistoryPromise;
  await downloadPromise;

  ok(!(await PlacesUtils.history.hasVisits("http://4hour.com")),
     "Pretend visit to 4hour.com should now be deleted");
  ok((await PlacesUtils.history.hasVisits("http://4hour10minutes.com")),
     "Pretend visit to 4hour10minutes.com should should still exist");
  if (hoursSinceMidnight > 4) {
    ok((await PlacesUtils.history.hasVisits("http://today.com")),
       "Pretend visit to today.com should still exist");
  }
  ok((await PlacesUtils.history.hasVisits("http://before-today.com")),
    "Pretend visit to before-today.com should still exist");

  await countEntries("4hour", "4hour form entry should be deleted", checkZero);
  await countEntries("4hour10minutes", "4hour10minutes form entry should still exist", checkOne);
  if (hoursSinceMidnight > 4)
    await countEntries("today", "today form entry should still exist", checkOne);
  await countEntries("b4today", "b4today form entry should still exist", checkOne);

  ok(!(await downloadExists(publicList, "fakefile-4-hour")), "<4 hour old download should now be deleted");
  ok((await downloadExists(publicList, "fakefile-4-hour-10-minutes")), "4 hour 10 minute download should still be present");
  ok((await downloadExists(publicList, "fakefile-old")), "Year old download should still be present");
  if (hoursSinceMidnight > 4)
    ok((await downloadExists(publicList, "fakefile-today")), "'Today' download should still be present");

  downloadPromise = promiseDownloadRemoved(publicList);
  formHistoryPromise = promiseFormHistoryRemoved();

  // Clear 4 hours 10 minutes
  range = [now_uSec - 250 * 60 * 1000000, now_uSec];
  await Sanitizer.sanitize(null, {range, ignoreTimespan: false});

  await formHistoryPromise;
  await downloadPromise;

  ok(!(await PlacesUtils.history.hasVisits("http://4hour10minutes.com")),
     "Pretend visit to 4hour10minutes.com should now be deleted");
  if (minutesSinceMidnight > 250) {
    ok((await PlacesUtils.history.hasVisits("http://today.com")),
       "Pretend visit to today.com should still exist");
  }
  ok((await PlacesUtils.history.hasVisits("http://before-today.com")),
    "Pretend visit to before-today.com should still exist");

  await countEntries("4hour10minutes", "4hour10minutes form entry should be deleted", checkZero);
  if (minutesSinceMidnight > 250)
    await countEntries("today", "today form entry should still exist", checkOne);
  await countEntries("b4today", "b4today form entry should still exist", checkOne);

  ok(!(await downloadExists(publicList, "fakefile-4-hour-10-minutes")), "4 hour 10 minute download should now be deleted");
  ok((await downloadExists(publicList, "fakefile-old")), "Year old download should still be present");
  if (minutesSinceMidnight > 250)
    ok((await downloadExists(publicList, "fakefile-today")), "'Today' download should still be present");

  // The 'Today' download might have been already deleted, in which case we
  // should not wait for a download removal notification.
  if (minutesSinceMidnight > 250) {
    downloadPromise = promiseDownloadRemoved(publicList);
    formHistoryPromise = promiseFormHistoryRemoved();
  } else {
    downloadPromise = formHistoryPromise = Promise.resolve();
  }

  // Clear Today
  Services.prefs.setIntPref(Sanitizer.PREF_TIMESPAN, 4);
  await Sanitizer.sanitize(null, {ignoreTimespan: false});

  await formHistoryPromise;
  await downloadPromise;

  // Be careful.  If we add our objectss just before midnight, and sanitize
  // runs immediately after, they won't be expired.  This is expected, but
  // we should not test in that case.  We cannot just test for opposite
  // condition because we could cross midnight just one moment after we
  // cache our time, then we would have an even worse random failure.
  var today = isToday(new Date(now_mSec));
  if (today) {
    ok(!(await PlacesUtils.history.hasVisits("http://today.com")),
       "Pretend visit to today.com should now be deleted");

    await countEntries("today", "today form entry should be deleted", checkZero);
    ok(!(await downloadExists(publicList, "fakefile-today")), "'Today' download should now be deleted");
  }

  ok((await PlacesUtils.history.hasVisits("http://before-today.com")),
     "Pretend visit to before-today.com should still exist");
  await countEntries("b4today", "b4today form entry should still exist", checkOne);
  ok((await downloadExists(publicList, "fakefile-old")), "Year old download should still be present");

  downloadPromise = promiseDownloadRemoved(publicList);
  formHistoryPromise = promiseFormHistoryRemoved();

  // Choose everything
  Services.prefs.setIntPref(Sanitizer.PREF_TIMESPAN, 0);
  await Sanitizer.sanitize(null, {ignoreTimespan: false});

  await formHistoryPromise;
  await downloadPromise;

  ok(!(await PlacesUtils.history.hasVisits("http://before-today.com")),
     "Pretend visit to before-today.com should now be deleted");

  await countEntries("b4today", "b4today form entry should be deleted", checkZero);

  ok(!(await downloadExists(publicList, "fakefile-old")), "Year old download should now be deleted");
}

async function setupHistory() {

  let places = [];

  function addPlace(aURI, aTitle, aVisitDate) {
    places.push({
      uri: aURI,
      title: aTitle,
      visitDate: aVisitDate,
      transition: Ci.nsINavHistoryService.TRANSITION_LINK
    });
  }

  addPlace("http://10minutes.com/", "10 minutes ago", now_uSec - 10 * kUsecPerMin);
  addPlace("http://1hour.com/", "Less than 1 hour ago", now_uSec - 45 * kUsecPerMin);
  addPlace("http://1hour10minutes.com/", "1 hour 10 minutes ago", now_uSec - 70 * kUsecPerMin);
  addPlace("http://2hour.com/", "Less than 2 hours ago", now_uSec - 90 * kUsecPerMin);
  addPlace("http://2hour10minutes.com/", "2 hours 10 minutes ago", now_uSec - 130 * kUsecPerMin);
  addPlace("http://4hour.com/", "Less than 4 hours ago", now_uSec - 180 * kUsecPerMin);
  addPlace("http://4hour10minutes.com/", "4 hours 10 minutesago", now_uSec - 250 * kUsecPerMin);

  let today = new Date();
  today.setHours(0);
  today.setMinutes(0);
  today.setSeconds(0);
  today.setMilliseconds(1);
  addPlace("http://today.com/", "Today", today.getTime() * 1000);

  let lastYear = new Date();
  lastYear.setFullYear(lastYear.getFullYear() - 1);
  addPlace("http://before-today.com/", "Before Today", lastYear.getTime() * 1000);
  await PlacesTestUtils.addVisits(places);
}

async function setupFormHistory() {

  function searchEntries(terms, params) {
    return new Promise((resolve, reject) => {

      let results = [];
      FormHistory.search(terms, params, { handleResult: result => results.push(result),
                                          handleError(error) {
                                            reject(error);
                                            throw new Error("Error occurred searching form history: " + error);
                                          },
                                          handleCompletion(reason) { resolve(results); }
                                        });
    });
  }

  function update(changes) {
    return new Promise((resolve, reject) => {
      FormHistory.update(changes, { handleError(error) {
                                      reject(error);
                                      throw new Error("Error occurred searching form history: " + error);
                                    },
                                    handleCompletion(reason) { resolve(); }
                                  });
    });
  }

  // Make sure we've got a clean DB to start with, then add the entries we'll be testing.
  await update(
    [{
        op: "remove"
     },
     {
        op: "add",
        fieldname: "10minutes",
        value: "10m"
      }, {
        op: "add",
        fieldname: "1hour",
        value: "1h"
      }, {
        op: "add",
        fieldname: "1hour10minutes",
        value: "1h10m"
      }, {
        op: "add",
        fieldname: "2hour",
        value: "2h"
      }, {
        op: "add",
        fieldname: "2hour10minutes",
        value: "2h10m"
      }, {
        op: "add",
        fieldname: "4hour",
        value: "4h"
      }, {
        op: "add",
        fieldname: "4hour10minutes",
        value: "4h10m"
      }, {
        op: "add",
        fieldname: "today",
        value: "1d"
      }, {
        op: "add",
        fieldname: "b4today",
        value: "1y"
      }]);

  // Artifically age the entries to the proper vintage.
  let timestamp = now_uSec - 10 * kUsecPerMin;
  let results = await searchEntries(["guid"], { fieldname: "10minutes" });
  await update({ op: "update", firstUsed: timestamp, guid: results[0].guid });

  timestamp = now_uSec - 45 * kUsecPerMin;
  results = await searchEntries(["guid"], { fieldname: "1hour" });
  await update({ op: "update", firstUsed: timestamp, guid: results[0].guid });

  timestamp = now_uSec - 70 * kUsecPerMin;
  results = await searchEntries(["guid"], { fieldname: "1hour10minutes" });
  await update({ op: "update", firstUsed: timestamp, guid: results[0].guid });

  timestamp = now_uSec - 90 * kUsecPerMin;
  results = await searchEntries(["guid"], { fieldname: "2hour" });
  await update({ op: "update", firstUsed: timestamp, guid: results[0].guid });

  timestamp = now_uSec - 130 * kUsecPerMin;
  results = await searchEntries(["guid"], { fieldname: "2hour10minutes" });
  await update({ op: "update", firstUsed: timestamp, guid: results[0].guid });

  timestamp = now_uSec - 180 * kUsecPerMin;
  results = await searchEntries(["guid"], { fieldname: "4hour" });
  await update({ op: "update", firstUsed: timestamp, guid: results[0].guid });

  timestamp = now_uSec - 250 * kUsecPerMin;
  results = await searchEntries(["guid"], { fieldname: "4hour10minutes" });
  await update({ op: "update", firstUsed: timestamp, guid: results[0].guid });

  let today = new Date();
  today.setHours(0);
  today.setMinutes(0);
  today.setSeconds(0);
  today.setMilliseconds(1);
  timestamp = today.getTime() * 1000;
  results = await searchEntries(["guid"], { fieldname: "today" });
  await update({ op: "update", firstUsed: timestamp, guid: results[0].guid });

  let lastYear = new Date();
  lastYear.setFullYear(lastYear.getFullYear() - 1);
  timestamp = lastYear.getTime() * 1000;
  results = await searchEntries(["guid"], { fieldname: "b4today" });
  await update({ op: "update", firstUsed: timestamp, guid: results[0].guid });

  var checks = 0;
  let checkOne = function(num, message) { is(num, 1, message); checks++; };

  // Sanity check.
  await countEntries("10minutes", "Checking for 10minutes form history entry creation", checkOne);
  await countEntries("1hour", "Checking for 1hour form history entry creation", checkOne);
  await countEntries("1hour10minutes", "Checking for 1hour10minutes form history entry creation", checkOne);
  await countEntries("2hour", "Checking for 2hour form history entry creation", checkOne);
  await countEntries("2hour10minutes", "Checking for 2hour10minutes form history entry creation", checkOne);
  await countEntries("4hour", "Checking for 4hour form history entry creation", checkOne);
  await countEntries("4hour10minutes", "Checking for 4hour10minutes form history entry creation", checkOne);
  await countEntries("today", "Checking for today form history entry creation", checkOne);
  await countEntries("b4today", "Checking for b4today form history entry creation", checkOne);
  is(checks, 9, "9 checks made");
}

async function setupDownloads() {

  let publicList = await Downloads.getList(Downloads.PUBLIC);

  let download = await Downloads.createDownload({
    source: "https://bugzilla.mozilla.org/show_bug.cgi?id=480169",
    target: "fakefile-10-minutes"
  });
  download.startTime = new Date(now_mSec - 10 * kMsecPerMin), // 10 minutes ago
  download.canceled = true;
  await publicList.add(download);

  download = await Downloads.createDownload({
    source: "https://bugzilla.mozilla.org/show_bug.cgi?id=453440",
    target: "fakefile-1-hour"
  });
  download.startTime = new Date(now_mSec - 45 * kMsecPerMin), // 45 minutes ago
  download.canceled = true;
  await publicList.add(download);

  download = await Downloads.createDownload({
    source: "https://bugzilla.mozilla.org/show_bug.cgi?id=480169",
    target: "fakefile-1-hour-10-minutes"
  });
  download.startTime = new Date(now_mSec - 70 * kMsecPerMin), // 70 minutes ago
  download.canceled = true;
  await publicList.add(download);

  download = await Downloads.createDownload({
    source: "https://bugzilla.mozilla.org/show_bug.cgi?id=453440",
    target: "fakefile-2-hour"
  });
  download.startTime = new Date(now_mSec - 90 * kMsecPerMin), // 90 minutes ago
  download.canceled = true;
  await publicList.add(download);

  download = await Downloads.createDownload({
    source: "https://bugzilla.mozilla.org/show_bug.cgi?id=480169",
    target: "fakefile-2-hour-10-minutes"
  });
  download.startTime = new Date(now_mSec - 130 * kMsecPerMin), // 130 minutes ago
  download.canceled = true;
  await publicList.add(download);

  download = await Downloads.createDownload({
    source: "https://bugzilla.mozilla.org/show_bug.cgi?id=453440",
    target: "fakefile-4-hour"
  });
  download.startTime = new Date(now_mSec - 180 * kMsecPerMin), // 180 minutes ago
  download.canceled = true;
  await publicList.add(download);

  download = await Downloads.createDownload({
    source: "https://bugzilla.mozilla.org/show_bug.cgi?id=480169",
    target: "fakefile-4-hour-10-minutes"
  });
  download.startTime = new Date(now_mSec - 250 * kMsecPerMin), // 250 minutes ago
  download.canceled = true;
  await publicList.add(download);

  // Add "today" download
  let today = new Date();
  today.setHours(0);
  today.setMinutes(0);
  today.setSeconds(0);
  today.setMilliseconds(1);

  download = await Downloads.createDownload({
    source: "https://bugzilla.mozilla.org/show_bug.cgi?id=453440",
    target: "fakefile-today"
  });
  download.startTime = today, // 12:00:01 AM this morning
  download.canceled = true;
  await publicList.add(download);

  // Add "before today" download
  let lastYear = new Date();
  lastYear.setFullYear(lastYear.getFullYear() - 1);

  download = await Downloads.createDownload({
    source: "https://bugzilla.mozilla.org/show_bug.cgi?id=453440",
    target: "fakefile-old"
  });
  download.startTime = lastYear,
  download.canceled = true;
  await publicList.add(download);

  // Confirm everything worked
  let downloads = await publicList.getAll();
  is(downloads.length, 9, "9 Pretend downloads added");

  ok((await downloadExists(publicList, "fakefile-old")), "Pretend download for everything case should exist");
  ok((await downloadExists(publicList, "fakefile-10-minutes")), "Pretend download for 10-minutes case should exist");
  ok((await downloadExists(publicList, "fakefile-1-hour")), "Pretend download for 1-hour case should exist");
  ok((await downloadExists(publicList, "fakefile-1-hour-10-minutes")), "Pretend download for 1-hour-10-minutes case should exist");
  ok((await downloadExists(publicList, "fakefile-2-hour")), "Pretend download for 2-hour case should exist");
  ok((await downloadExists(publicList, "fakefile-2-hour-10-minutes")), "Pretend download for 2-hour-10-minutes case should exist");
  ok((await downloadExists(publicList, "fakefile-4-hour")), "Pretend download for 4-hour case should exist");
  ok((await downloadExists(publicList, "fakefile-4-hour-10-minutes")), "Pretend download for 4-hour-10-minutes case should exist");
  ok((await downloadExists(publicList, "fakefile-today")), "Pretend download for Today case should exist");
}

/**
 * Checks to see if the downloads with the specified id exists.
 *
 * @param aID
 *        The ids of the downloads to check.
 */
let downloadExists = async function(list, path) {
  let listArray = await list.getAll();
  return listArray.some(i => i.target.path == path);
};

function isToday(aDate) {
  return aDate.getDate() == new Date().getDate();
}
