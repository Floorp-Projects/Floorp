/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Components.utils.import("resource://gre/modules/AlarmService.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gTimeService",
                                   "@mozilla.org/time/timeservice;1",
                                   "nsITimeService");

const ALARM_OFFSET = 10000; // 10 seconds.
const CLOCK_OFFSET = 20000; // 20 seconds.
const MANIFEST_URL = "http://dummyurl.com/manifest.webapp";

let alarmDate;
let alarmFired;
function alarmCb() {
  alarmFired = true;
};

function run_test() {
  do_get_profile();
  Services.prefs.setBoolPref("dom.mozAlarms.enabled", true);
  run_next_test();
}

/* Tests */

add_test(function test_getAll() {
  do_print("= There should not be any alarm =");
  AlarmService._db.getAll(MANIFEST_URL, (aAlarms) => {
    do_check_eq(aAlarms.length, 0);
    run_next_test();
  });
});

add_test(function test_addAlarm() {
  do_print("= Set alarm =");
  alarmDate = Date.now() + ALARM_OFFSET;
  AlarmService.add({
    date: alarmDate,
    manifestURL: MANIFEST_URL
  }, alarmCb, run_next_test, do_throw);
});

add_test(function test_alarmNotFired() {
  do_print("= The alarm should be in the DB and pending =");
  AlarmService._db.getAll(MANIFEST_URL, aAlarms => {
    do_check_eq(aAlarms.length, 1, "The alarm is in the DB");
    run_next_test();
  });
});

add_test(function test_changeSystemClock() {
  do_print("= Change system clock =");
  gTimeService.set(Date.now() + CLOCK_OFFSET);
  run_next_test();
});

add_test(function test_alarmFired() {
  do_print("= The alarm should have been fired and removed from the DB =");
  do_check_true(alarmFired, "The alarm was fired");
  AlarmService._db.getAll(MANIFEST_URL, aAlarms => {
    do_check_eq(aAlarms.length, 0, "No alarms in the DB");
    run_next_test();
  });
});
