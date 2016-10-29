"use strict";

/**
 * This test is for the undoCloseById function.
 */

Cu.import("resource:///modules/sessionstore/SessionStore.jsm");

function openAndCloseTab(window, url) {
  let tab = window.gBrowser.addTab(url);
  yield promiseBrowserLoaded(tab.linkedBrowser, true, url);
  yield TabStateFlusher.flush(tab.linkedBrowser);
  yield promiseRemoveTab(tab);
}

function* openWindow(url) {
  let win = yield promiseNewWindowLoaded();
  let flags = Ci.nsIWebNavigation.LOAD_FLAGS_REPLACE_HISTORY;
  win.gBrowser.selectedBrowser.loadURIWithFlags(url, flags);
  yield promiseBrowserLoaded(win.gBrowser.selectedBrowser, true, url);
  // Wait 20 ms because if we close this window immediately it sometimes
  // is not recorded by SessionStorage.
  yield new Promise(resolve => setTimeout(resolve, 20));
  return win;
}

add_task(function* test_undoCloseById() {
  // Clear the lists of closed windows and tabs.
  forgetClosedWindows();
  while (SessionStore.getClosedTabCount(window)) {
    SessionStore.forgetClosedTab(window, 0);
  }

  // Open a new window.
  let win = yield openWindow("about:robots");

  // Open and close a tab.
  yield openAndCloseTab(win, "about:mozilla");

  // Record the first closedId created.
  let initialClosedId = SessionStore.getClosedTabData(win, false)[0].closedId;

  // Open and close another tab.
  yield openAndCloseTab(win, "about:robots");  // closedId == initialClosedId + 1

  // Undo closing the second tab.
  let tab = SessionStore.undoCloseById(initialClosedId + 1);
  yield promiseBrowserLoaded(tab.linkedBrowser);
  is(tab.linkedBrowser.currentURI.spec, "about:robots", "The expected tab was re-opened");

  let notTab = SessionStore.undoCloseById(initialClosedId + 1);
  is(notTab, undefined, "Re-opened tab cannot be unClosed again by closedId");

  // Undo closing the first tab.
  let tab2 = SessionStore.undoCloseById(initialClosedId);
  yield promiseBrowserLoaded(tab2.linkedBrowser);
  is(tab2.linkedBrowser.currentURI.spec, "about:mozilla", "The expected tab was re-opened");

  // Close the two tabs we re-opened.
  yield promiseRemoveTab(tab); // closedId == initialClosedId + 2
  yield promiseRemoveTab(tab2); // closedId == initialClosedId + 3

  // Open another new window.
  let win2 = yield openWindow("about:mozilla");

  // Close both windows.
  yield BrowserTestUtils.closeWindow(win); // closedId == initialClosedId + 4
  yield BrowserTestUtils.closeWindow(win2); // closedId == initialClosedId + 5

  // Undo closing the second window.
  win = SessionStore.undoCloseById(initialClosedId + 5);
  yield BrowserTestUtils.waitForEvent(win, "load");

  // Make sure we wait until this window is restored.
  yield BrowserTestUtils.waitForEvent(win.gBrowser.tabContainer,
                                      "SSTabRestored");

  is(win.gBrowser.selectedBrowser.currentURI.spec, "about:mozilla", "The expected window was re-opened");

  let notWin = SessionStore.undoCloseById(initialClosedId + 5);
  is(notWin, undefined, "Re-opened window cannot be unClosed again by closedId");

  // Close the window again.
  yield BrowserTestUtils.closeWindow(win);

  // Undo closing the first window.
  win = SessionStore.undoCloseById(initialClosedId + 4);

  yield BrowserTestUtils.waitForEvent(win, "load");

  // Make sure we wait until this window is restored.
  yield BrowserTestUtils.waitForEvent(win.gBrowser.tabContainer,
                                      "SSTabRestored");

  is(win.gBrowser.selectedBrowser.currentURI.spec, "about:robots", "The expected window was re-opened");

  // Close the window again.
  yield BrowserTestUtils.closeWindow(win);
});
