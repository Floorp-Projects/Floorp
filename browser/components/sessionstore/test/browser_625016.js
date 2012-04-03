/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let newWin;
let newTab;

function test() {
  /** Test for Bug 625016 - Restore windows closed in succession to quit (non-OSX only) **/

  // We'll test this by opening a new window, waiting for the save event, then
  // closing that window. We'll observe the "sessionstore-state-write" notification
  // and check that the state contains no _closedWindows. We'll then add a new
  // tab and make sure that the state following that was reset and the closed
  // window is now in _closedWindows.

  waitForExplicitFinish();
  requestLongerTimeout(2);

  // We speed up the interval between session saves to ensure that the test
  // runs quickly.
  Services.prefs.setIntPref("browser.sessionstore.interval", 4000);
  registerCleanupFunction(function () {
    Services.prefs.clearUserPref("browser.sessionstore.interval");
  });

  waitForSaveState(setup);
}

function setup() {
  // We'll clear all closed windows to make sure our state is clean
  // forgetClosedWindow doesn't trigger a delayed save
  while (ss.getClosedWindowCount()) {
    ss.forgetClosedWindow(0);
  }
  is(ss.getClosedWindowCount(), 0, "starting with no closed windows");

  // Open a new window, which should trigger a save event soon.
  waitForSaveState(onSaveState);
  newWin = openDialog(location, "_blank", "chrome,all,dialog=no", "about:rights");
}

function onSaveState() {
  // Double check that we have no closed windows
  is(ss.getClosedWindowCount(), 0, "no closed windows on first save");

  Services.obs.addObserver(observe1, "sessionstore-state-write", false);

  // Now close the new window, which should trigger another save event
  newWin.close();
}

function observe1(aSubject, aTopic, aData) {
  info("observe1: " + aTopic);
  switch (aTopic) {
    case "sessionstore-state-write":
      aSubject.QueryInterface(Ci.nsISupportsString);
      let state = JSON.parse(aSubject.data);
      is(state.windows.length, 2,
         "observe1: 2 windows in data being writted to disk");
      is(state._closedWindows.length, 0,
         "observe1: no closed windows in data being writted to disk");

      // The API still treats the closed window as closed, so ensure that window is there
      is(ss.getClosedWindowCount(), 1,
         "observe1: 1 closed window according to API");
      Services.obs.removeObserver(observe1, "sessionstore-state-write", false);
      Services.obs.addObserver(observe1, "sessionstore-state-write-complete", false);
      break;
    case "sessionstore-state-write-complete":
      Services.obs.removeObserver(observe1, "sessionstore-state-write-complete", false);
      openTab();
      break;
  }
}

function observe2(aSubject, aTopic, aData) {
  info("observe2: " + aTopic);
  switch (aTopic) {
    case "sessionstore-state-write":
      aSubject.QueryInterface(Ci.nsISupportsString);
      let state = JSON.parse(aSubject.data);
      is(state.windows.length, 1,
         "observe2: 1 window in data being writted to disk");
      is(state._closedWindows.length, 1,
         "observe2: 1 closed window in data being writted to disk");

      // The API still treats the closed window as closed, so ensure that window is there
      is(ss.getClosedWindowCount(), 1,
         "observe2: 1 closed window according to API");
      Services.obs.removeObserver(observe2, "sessionstore-state-write", false);
      Services.obs.addObserver(observe2, "sessionstore-state-write-complete", false);
      break;
    case "sessionstore-state-write-complete":
      Services.obs.removeObserver(observe2, "sessionstore-state-write-complete", false);
      done();
      break;
  }
}

// We'll open a tab, which should trigger another state save which would wipe
// the _shouldRestore attribute from the closed window
function openTab() {
  Services.obs.addObserver(observe2, "sessionstore-state-write", false);
  newTab = gBrowser.addTab("about:mozilla");
}

function done() {
  gBrowser.removeTab(newTab);
  // The API still represents the closed window as closed, so we can clear it
  // with the API, but just to make sure...
  is(ss.getClosedWindowCount(), 1, "1 closed window according to API");
  ss.forgetClosedWindow(0);
  executeSoon(finish);
}

