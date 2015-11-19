/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TESTS = [
  { url: "about:config",
    key: "bug 394759 Non-PB",
    value: "uniq" + r() },
  { url: "about:mozilla",
    key: "bug 394759 PB",
    value: "uniq" + r() },
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
    let data = JSON.parse(ss.getClosedWindowData())[0];
    is(ss.getClosedWindowCount(), 1, "Check that the closed window count hasn't changed");
    ok(JSON.stringify(data).indexOf(aValue) > -1,
       "Check the closed window data was stored correctly");
    registerCleanupFunction(() => promiseWindowClosed(win));
  });
}

add_task(function* init() {
  forgetClosedWindows();
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

