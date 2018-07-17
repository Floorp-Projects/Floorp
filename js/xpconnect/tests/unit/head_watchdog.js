/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// Pref management.
//

ChromeUtils.import("resource://testing-common/PromiseTestUtils.jsm");

///////////////////
//
// Whitelisting these tests.
// As part of bug 1077403, the shutdown crash should be fixed.
//
// These tests may crash intermittently on shutdown if the DOM Promise uncaught
// rejection observers are still registered when the watchdog operates.
PromiseTestUtils.thisTestLeaksUncaughtRejectionsAndShouldBeFixed();

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
  return oldTimeout;
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
  tm.dispatchToMainThread({run: fn});
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

function checkWatchdog(expectInterrupt) {
  var oldTimeout = setScriptTimeout(1);
  var lastWatchdogWakeup = Cu.getWatchdogTimestamp("WatchdogWakeup");

  return new Promise(resolve => {
    let inBusyWait = false;
    setInterruptCallback(function() {
      // If the watchdog didn't actually trigger the operation callback, ignore
      // this call. This allows us to test the actual watchdog behavior without
      // interference from other sites where we trigger the operation callback.
      if (lastWatchdogWakeup == Cu.getWatchdogTimestamp("WatchdogWakeup")) {
        return true;
      }
      if (!inBusyWait) {
        Assert.ok(true, "Not in busy wait, ignoring interrupt callback");
        return true;
      }

      Assert.ok(expectInterrupt, "Interrupt callback fired");
      setInterruptCallback(undefined);
      setScriptTimeout(oldTimeout);
      // Schedule the promise for resolution before we kill this script.
      executeSoon(resolve);
      return false;
    });

    executeSoon(function() {
      inBusyWait = true;
      busyWait(3000);
      inBusyWait = false;
      Assert.ok(!expectInterrupt, "Interrupt callback didn't fire");
      setInterruptCallback(undefined);
      setScriptTimeout(oldTimeout);
      resolve();
    });
  });
}

function run_test() {

  // Run async.
  do_test_pending();

  // Run the async function.
  testBody().then(() => {
    do_test_finished();
  });
}

