/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const IS_MAC = ("nsILocalFileMac" in Ci);
const URL_PREFIX = "about:mozilla?t=browser_revive_windows&r=";
const PREF_MAX_UNDO = "browser.sessionstore.max_windows_undo";

const URL_MAIN_WINDOW = URL_PREFIX + Math.random();
const URL_ADD_WINDOW1 = URL_PREFIX + Math.random();
const URL_ADD_WINDOW2 = URL_PREFIX + Math.random();
const URL_CLOSED_WINDOW = URL_PREFIX + Math.random();

add_task(function* setup() {
  registerCleanupFunction(() => Services.prefs.clearUserPref(PREF_MAX_UNDO));
});

/**
 * This test ensure that when closing windows in succession until the browser
 * quits we are able to revive more windows than we keep around for the
 * "Undo Close Window" feature.
 */
add_task(function* test_revive_windows() {
  // We can restore a single window max.
  Services.prefs.setIntPref(PREF_MAX_UNDO, 1);

  // Clear list of closed windows.
  forgetClosedWindows();

  let windows = [];

  // Create three windows.
  for (let i = 0; i < 3; i++) {
    let win = yield promiseNewWindow();
    windows.push(win);

    let tab = win.gBrowser.addTab("about:mozilla");
    yield promiseBrowserLoaded(tab.linkedBrowser);
  }

  // Close all windows.
  for (let win of windows) {
    yield promiseWindowClosed(win);
  }

  is(ss.getClosedWindowCount(), 1, "one window restorable");

  // Save to disk and read.
  let state = JSON.parse(yield promiseRecoveryFileContents());

  // Check number of windows.
  if (IS_MAC) {
    is(state.windows.length, 1, "one open window");
    is(state._closedWindows.length, 1, "one closed window");
  } else {
    is(state.windows.length, 4, "four open windows");
    is(state._closedWindows.length, 0, "closed windows");
  }
});

/**
 * This test ensures that when closing windows one after the other until the
 * browser shuts down (on Windows and Linux) we revive closed windows in the
 * right order.
 */
add_task(function* test_revive_windows_order() {
  // We can restore up to three windows max.
  Services.prefs.setIntPref(PREF_MAX_UNDO, 3);

  // Clear list of closed windows.
  forgetClosedWindows();

  let tab = gBrowser.addTab(URL_MAIN_WINDOW);
  yield promiseBrowserLoaded(tab.linkedBrowser);
  registerCleanupFunction(() => gBrowser.removeTab(tab));

  let win0 = yield promiseNewWindow();
  let tab0 = win0.gBrowser.addTab(URL_CLOSED_WINDOW);
  yield promiseBrowserLoaded(tab0.linkedBrowser);

  yield promiseWindowClosed(win0);
  let data = ss.getClosedWindowData();
  ok(data.contains(URL_CLOSED_WINDOW), "window is restorable");

  let win1 = yield promiseNewWindow();
  let tab1 = win1.gBrowser.addTab(URL_ADD_WINDOW1);
  yield promiseBrowserLoaded(tab1.linkedBrowser);

  let win2 = yield promiseNewWindow();
  let tab2 = win2.gBrowser.addTab(URL_ADD_WINDOW2);
  yield promiseBrowserLoaded(tab2.linkedBrowser);

  // Close both windows so that |win1| will be opened first and would be
  // behind |win2| that was closed later.
  yield promiseWindowClosed(win1);
  yield promiseWindowClosed(win2);

  // Repeat the checks once.
  for (let i = 0; i < 2; i++) {
    info(`checking window data, iteration #${i}`);
    let contents = yield promiseRecoveryFileContents();
    let {windows, _closedWindows: closedWindows} = JSON.parse(contents);

    if (IS_MAC) {
      // Check number of windows.
      is(windows.length, 1, "one open window");
      is(closedWindows.length, 3, "three closed windows");

      // Check open window.
      ok(JSON.stringify(windows).contains(URL_MAIN_WINDOW),
        "open window is correct");

      // Check closed windows.
      ok(JSON.stringify(closedWindows[0]).contains(URL_ADD_WINDOW2),
        "correct first additional window");
      ok(JSON.stringify(closedWindows[1]).contains(URL_ADD_WINDOW1),
        "correct second additional window");
      ok(JSON.stringify(closedWindows[2]).contains(URL_CLOSED_WINDOW),
        "correct main window");
    } else {
      // Check number of windows.
      is(windows.length, 3, "three open windows");
      is(closedWindows.length, 1, "one closed window");

      // Check closed window.
      ok(JSON.stringify(closedWindows).contains(URL_CLOSED_WINDOW),
        "closed window is correct");

      // Check that windows are in the right order.
      ok(JSON.stringify(windows[0]).contains(URL_ADD_WINDOW1),
        "correct first additional window");
      ok(JSON.stringify(windows[1]).contains(URL_ADD_WINDOW2),
        "correct second additional window");
      ok(JSON.stringify(windows[2]).contains(URL_MAIN_WINDOW),
        "correct main window");
    }
  }
});

function promiseNewWindow() {
  return new Promise(resolve => whenNewWindowLoaded({private: false}, resolve));
}

function forgetClosedWindows() {
  while (ss.getClosedWindowCount()) {
    ss.forgetClosedWindow(0);
  }
}
