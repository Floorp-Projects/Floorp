"use strict";

/**
 * This test ensures that after closing a window we keep its state data around
 * as long as something keeps a reference to it. It should only be possible to
 * read data after closing - writing should fail.
 */

add_task(function* test() {
  // Open a new window.
  let win = yield promiseNewWindowLoaded();

  // Load some URL in the current tab.
  let flags = Ci.nsIWebNavigation.LOAD_FLAGS_REPLACE_HISTORY;
  win.gBrowser.selectedBrowser.loadURIWithFlags("about:robots", flags);
  yield promiseBrowserLoaded(win.gBrowser.selectedBrowser);

  // Open a second tab and close the first one.
  let tab = win.gBrowser.addTab("about:mozilla");
  yield promiseBrowserLoaded(tab.linkedBrowser);
  yield TabStateFlusher.flush(tab.linkedBrowser);
  yield promiseRemoveTab(win.gBrowser.tabs[0]);

  // Make sure our window is still tracked by sessionstore
  // and the window state is as expected.
  ok("__SSi" in win, "window is being tracked by sessionstore");
  ss.setWindowValue(win, "foo", "bar");
  checkWindowState(win);

  let state = ss.getWindowState(win);
  let closedTabData = ss.getClosedTabData(win);

  // Close our window.
  yield BrowserTestUtils.closeWindow(win);

  // SessionStore should no longer track our window
  // but it should still report the same state.
  ok(!("__SSi" in win), "sessionstore does no longer track our window");
  checkWindowState(win);

  // Make sure we're not allowed to modify state data.
  Assert.throws(() => ss.setWindowState(win, {}),
    "we're not allowed to modify state data anymore");
  Assert.throws(() => ss.setWindowValue(win, "foo", "baz"),
    "we're not allowed to modify state data anymore");
});

function checkWindowState(window) {
  let {windows: [{tabs}]} = JSON.parse(ss.getWindowState(window));
  is(tabs.length, 1, "the window has a single tab");
  is(tabs[0].entries[0].url, "about:mozilla", "the tab is about:mozilla");

  is(ss.getClosedTabCount(window), 1, "the window has one closed tab");
  let [{state: {entries: [{url}]}}] = JSON.parse(ss.getClosedTabData(window));
  is(url, "about:robots", "the closed tab is about:robots");

  is(ss.getWindowValue(window, "foo"), "bar", "correct extData value");
}

function shouldThrow(f) {
  try {
    f();
  } catch (e) {
    return true;
  }
}
