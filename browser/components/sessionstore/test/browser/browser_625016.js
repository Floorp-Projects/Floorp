/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let newWin;

function test() {
  /** Test for Bug 625016 - Restore windows closed in succession to quit (non-OSX only) **/

  // We'll test this by opening a new window, waiting for the save event, then
  // closing that window. We'll observe the "sessionstore-state-write" notification
  // and check that the state contains no _closedWindows.

  waitForExplicitFinish();

  registerCleanupFunction(function() {
  });

  // We speed up the interval between session saves to ensure that the test
  // runs quickly.
  Services.prefs.setIntPref("browser.sessionstore.interval", 2000);

  // We'll clear all closed windows to make sure our state is clean
  while(SS_SVC.getClosedWindowCount()) {
    SS_SVC.forgetClosedWindow();
  }

  // Open a new window, which should trigger a save event soon.
  waitForSaveState(onSaveState);
  newWin = openDialog(location, "_blank", "chrome,all,dialog=no", "about:robots");
}

function onSaveState() {
  // Double check that we have no closed windows
  is(SS_SVC.countClosedWindows(), 0, "no closed windows on first save");
  Services.obs.addObserver(observe, "sessionstore-state-write", true);

  // Now close the new window, which should trigger another save event
  newWin.close();
}

// Our observer for "sessionstore-write-data"
function observe(aSubject, aTopic, aData) {
  dump(aSubject);
  dump(aData);
  let state = JSON.parse(aData);
  done();
}


function done() {
  Service.prefs.clearUserPref("browser.sessionstore.interval");
  finish();
}
