/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/** Private Browsing Test for Bug 394759 **/

let closedWindowCount = 0;
// Prevent VM timers issues, cache now and increment it manually.
let now = Date.now();

const TESTS = [
  { url: "about:config",
    key: "bug 394759 Non-PB",
    value: "uniq" + (++now) },
  { url: "about:mozilla",
    key: "bug 394759 PB",
    value: "uniq" + (++now) },
];

function promiseTestOpenCloseWindow(aIsPrivate, aTest) {
  return Task.spawn(function*() {
    let win = yield promiseNewWindowLoaded({ "private": aIsPrivate });
    win.gBrowser.selectedBrowser.loadURI(aTest.url);
    yield promiseBrowserLoaded(win.gBrowser.selectedBrowser);
    yield Promise.resolve();
    // Mark the window with some unique data to be restored later on.
    ss.setWindowValue(win, aTest.key, aTest.value);
    // Close.
    yield promiseWindowClosed(win);
  });
}

function promiseTestOnWindow(aIsPrivate, aValue) {
  return Task.spawn(function*() {
    let win = yield promiseNewWindowLoaded({ "private": aIsPrivate });
    yield promiseCheckClosedWindows(aIsPrivate, aValue);
    registerCleanupFunction(() => promiseWindowClosed(win));
  });
}

function promiseCheckClosedWindows(aIsPrivate, aValue) {
  return Task.spawn(function*() {
    let data = JSON.parse(ss.getClosedWindowData())[0];
    is(ss.getClosedWindowCount(), 1, "Check that the closed window count hasn't changed");
    ok(JSON.stringify(data).indexOf(aValue) > -1,
       "Check the closed window data was stored correctly");
  });
}

function promiseBlankState() {
  return Task.spawn(function*() {
    // Set interval to a large time so state won't be written while we setup
    // environment.
    Services.prefs.setIntPref("browser.sessionstore.interval", 100000);
    registerCleanupFunction(() =>  Services.prefs.clearUserPref("browser.sessionstore.interval"));

    // Set up the browser in a blank state. Popup windows in previous tests
    // result in different states on different platforms.
    let blankState = JSON.stringify({
      windows: [{
        tabs: [{ entries: [{ url: "about:blank" }] }],
        _closedTabs: []
      }],
      _closedWindows: []
    });

    ss.setBrowserState(blankState);

    // Wait for the sessionstore.js file to be written before going on.
    // Note: we don't wait for the complete event, since if asyncCopy fails we
    // would timeout.

    yield forceSaveState();
    closedWindowCount = ss.getClosedWindowCount();
    is(closedWindowCount, 0, "Correctly set window count");

    // Remove the sessionstore.js file before setting the interval to 0
    yield SessionFile.wipe();

    // Make sure that sessionstore.js can be forced to be created by setting
    // the interval pref to 0.
    yield forceSaveState();
  });
}

add_task(function* init() {
  while (ss.getClosedWindowCount() > 0) {
    ss.forgetClosedWindow(0);
  }
  while (ss.getClosedTabCount(window) > 0) {
    ss.forgetClosedTab(window, 0);
  }
});

add_task(function* main() {
  yield promiseTestOpenCloseWindow(false, TESTS[0]);
  yield promiseTestOpenCloseWindow(true, TESTS[1]);
  yield promiseTestOnWindow(false, TESTS[0].value);
  yield promiseTestOnWindow(true, TESTS[0].value);
});

