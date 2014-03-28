/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function* setup() {
  /** Test for Bug 625016 - Restore windows closed in succession to quit (non-OSX only) **/

  // We'll test this by opening a new window, waiting for the save
  // event, then closing that window. We'll observe the
  // "sessionstore-state-write-complete" notification and check that
  // the state contains no _closedWindows. We'll then add a new tab
  // and make sure that the state following that was reset and the
  // closed window is now in _closedWindows.

  requestLongerTimeout(2);

  yield forceSaveState();

  // We'll clear all closed windows to make sure our state is clean
  // forgetClosedWindow doesn't trigger a delayed save
  while (ss.getClosedWindowCount()) {
    ss.forgetClosedWindow(0);
  }
  is(ss.getClosedWindowCount(), 0, "starting with no closed windows");
});

add_task(function* new_window() {
  let newWin;
  try {
    newWin = yield promiseNewWindowLoaded();
    let tab = newWin.gBrowser.addTab("http://example.com/browser_625016.js?" + Math.random());
    yield promiseBrowserLoaded(tab.linkedBrowser);

    // Double check that we have no closed windows
    is(ss.getClosedWindowCount(), 0, "no closed windows on first save");

    yield promiseWindowClosed(newWin);
    newWin = null;

    let state = JSON.parse((yield promiseSaveFileContents()));
    is(state.windows.length, 2,
      "observe1: 2 windows in data written to disk");
    is(state._closedWindows.length, 0,
      "observe1: no closed windows in data written to disk");

    // The API still treats the closed window as closed, so ensure that window is there
    is(ss.getClosedWindowCount(), 1,
      "observe1: 1 closed window according to API");
  } finally {
    if (newWin) {
      yield promiseWindowClosed(newWin);
    }
    yield forceSaveState();
  }
});

// We'll open a tab, which should trigger another state save which would wipe
// the _shouldRestore attribute from the closed window
add_task(function* new_tab() {
  let newTab;
  try {
    newTab = gBrowser.addTab("about:mozilla");

    let state = JSON.parse((yield promiseSaveFileContents()));
    is(state.windows.length, 1,
      "observe2: 1 window in data being written to disk");
    is(state._closedWindows.length, 1,
      "observe2: 1 closed window in data being written to disk");

    // The API still treats the closed window as closed, so ensure that window is there
    is(ss.getClosedWindowCount(), 1,
      "observe2: 1 closed window according to API");
  } finally {
    gBrowser.removeTab(newTab);
  }
});


add_task(function* done() {
  // The API still represents the closed window as closed, so we can clear it
  // with the API, but just to make sure...
//  is(ss.getClosedWindowCount(), 1, "1 closed window according to API");
  while (ss.getClosedWindowCount()) {
    ss.forgetClosedWindow(0);
  }
  Services.prefs.clearUserPref("browser.sessionstore.interval");
});
