/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// Pref management.
//

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

var gPrefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);

function setWatchdogEnabled(enabled) {
  gPrefs.setBoolPref("dom.use_watchdog", enabled);
}

function isWatchdogEnabled() {
  return gPrefs.getBoolPref("dom.use_watchdog");
}

function setScriptTimeout(seconds) {
  var oldTimeout = gPrefs.getIntPref("dom.max_script_run_time");
  gPrefs.setIntPref("dom.max_script_run_time", seconds);
}

//
// Utilities.
//

function busyWait(ms) {
  var start = new Date();
  while ((new Date()) - start < ms) {}
}

function do_log_info(aMessage)
{
  print("TEST-INFO | " + _TEST_FILE + " | " + aMessage);
}

// We don't use do_execute_soon, because that inserts a
// do_test_{pending,finished} pair that gets screwed up when we terminate scripts
// from the operation callback.
function executeSoon(fn) {
  var tm = Cc["@mozilla.org/thread-manager;1"].getService(Ci.nsIThreadManager);
  tm.mainThread.dispatch({run: fn}, Ci.nsIThread.DISPATCH_NORMAL);
}

//
// Asynchronous watchdog diagnostics.
//
// When running, the watchdog wakes up every second, and fires the operation
// callback if the script has been running for >= the minimum script timeout.
// As such, if the script timeout is 1 second, a script should never be able to
// run for two seconds or longer without servicing the operation callback.
// We wait 3 seconds, just to be safe.
//

function checkWatchdog(expectInterrupt, continuation) {
  var oldTimeout = setScriptTimeout(1);
  var lastWatchdogWakeup = Cu.getWatchdogTimestamp("WatchdogWakeup");
  setInterruptCallback(function() {
    // If the watchdog didn't actually trigger the operation callback, ignore
    // this call. This allows us to test the actual watchdog behavior without
    // interference from other sites where we trigger the operation callback.
    if (lastWatchdogWakeup == Cu.getWatchdogTimestamp("WatchdogWakeup")) {
      return true;
    }
    do_check_true(expectInterrupt);
    setInterruptCallback(undefined);
    setScriptTimeout(oldTimeout);
    // Schedule our continuation before we kill this script.
    executeSoon(continuation);
    return false;
  });
  executeSoon(function() {
    busyWait(3000);
    do_check_true(!expectInterrupt);
    setInterruptCallback(undefined);
    setScriptTimeout(oldTimeout);
    continuation();
  });
}

var gGenerator;
function continueTest() {
  gGenerator.next();
}

function run_test() {

  // Run async.
  do_test_pending();

  // Instantiate the generator and kick it off.
  gGenerator = testBody();
  gGenerator.next();
}

