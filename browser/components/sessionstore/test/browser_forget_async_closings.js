"use strict";

const PAGE = "http://example.com/";

/**
 * Creates a tab in the current window worth storing in the
 * closedTabs array, and then closes it. Runs a synchronous
 * forgetFn passed in that should cause us to forget the tab,
 * and then ensures that after the tab has sent its final
 * update message that we didn't accidentally store it in
 * the closedTabs array.
 *
 * @param forgetFn (function)
 *        A synchronous function that should cause the tab
 *        to be forgotten.
 * @returns Promise
 */
let forgetTabHelper = Task.async(function*(forgetFn) {
  // We want to suppress all non-final updates from the browser tabs
  // so as to eliminate any racy-ness with this test.
  yield pushPrefs(["browser.sessionstore.debug.no_auto_updates", true]);

  // Forget any previous closed tabs from other tests that may have
  // run in the same session.
  Services.obs.notifyObservers(null, "browser:purge-session-history", 0);

  is(ss.getClosedTabCount(window), 0,
     "We should have 0 closed tabs being stored.");

  // Create a tab worth remembering.
  let tab = gBrowser.addTab(PAGE);
  let browser = tab.linkedBrowser;
  yield BrowserTestUtils.browserLoaded(browser, false, PAGE);
  yield TabStateFlusher.flush(browser);

  // Now close the tab, and immediately choose to forget it.
  let promise = BrowserTestUtils.removeTab(tab);

  // At this point, the tab will have closed, but the final update
  // to SessionStore hasn't come up yet. Now do the operation that
  // should cause us to forget the tab.
  forgetFn();

  is(ss.getClosedTabCount(window), 0, "Should have forgotten the closed tab");

  // Now wait for the final update to come up.
  yield promise;

  is(ss.getClosedTabCount(window), 0,
     "Should not have stored the forgotten closed tab");
});

/**
 * Creates a new window worth storing in the closeWIndows array,
 * and then closes it. Runs a synchronous forgetFn passed in that
 * should cause us to forget the window, and then ensures that after
 * the window has sent its final update message that we didn't
 * accidentally store it in the closedWindows array.
 *
 * @param forgetFn (function)
 *        A synchronous function that should cause the window
 *        to be forgotten.
 * @returns Promise
 */
let forgetWinHelper = Task.async(function*(forgetFn) {
  // We want to suppress all non-final updates from the browser tabs
  // so as to eliminate any racy-ness with this test.
  yield pushPrefs(["browser.sessionstore.debug.no_auto_updates", true]);

  // Forget any previous closed windows from other tests that may have
  // run in the same session.
  Services.obs.notifyObservers(null, "browser:purge-session-history", 0);

  is(ss.getClosedWindowCount(), 0, "We should have 0 closed windows being stored.");

  let newWin = yield BrowserTestUtils.openNewBrowserWindow();

  // Create a tab worth remembering.
  let tab = newWin.gBrowser.selectedTab;
  let browser = tab.linkedBrowser;
  browser.loadURI(PAGE);
  yield BrowserTestUtils.browserLoaded(browser, false, PAGE);
  yield TabStateFlusher.flush(browser);

  // Now close the window and immediately choose to forget it.
  let windowClosed = BrowserTestUtils.windowClosed(newWin);
  let domWindowClosed = BrowserTestUtils.domWindowClosed(newWin);

  newWin.close();
  yield domWindowClosed;

  // At this point, the window will have closed and the onClose handler
  // has run, but the final update  to SessionStore hasn't come up yet.
  // Now do the oepration that should cause us to forget the window.
  forgetFn();

  is(ss.getClosedWindowCount(), 0, "Should have forgotten the closed window");

  // Now wait for the final update to come up.
  yield windowClosed;

  is(ss.getClosedWindowCount(), 0, "Should not have stored the closed window");
});

/**
 * Tests that if we choose to forget a tab while waiting for its
 * final flush to complete, we don't accidentally store it.
 */
add_task(function* test_forget_closed_tab() {
  yield forgetTabHelper(() => {
    ss.forgetClosedTab(window, 0);
  });
});

/**
 * Tests that if we choose to forget a tab while waiting for its
 * final flush to complete, we don't accidentally store it.
 */
add_task(function* test_forget_closed_window() {
  yield forgetWinHelper(() => {
    ss.forgetClosedWindow(0);
  });
});

/**
 * Tests that if we choose to purge history while waiting for a
 * final flush of a tab to complete, we don't accidentally store it.
 */
add_task(function* test_forget_purged_tab() {
  yield forgetTabHelper(() => {
    Services.obs.notifyObservers(null, "browser:purge-session-history", 0);
  });
});

/**
 * Tests that if we choose to purge history while waiting for a
 * final flush of a window to complete, we don't accidentally
 * store it.
 */
add_task(function* test_forget_purged_window() {
  yield forgetWinHelper(() => {
    Services.obs.notifyObservers(null, "browser:purge-session-history", 0);
  });
});
