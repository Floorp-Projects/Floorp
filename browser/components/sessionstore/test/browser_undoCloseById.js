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
  return win;
}

function closeWindow(win) {
  yield BrowserTestUtils.closeWindow(win);
  // Wait 20 ms to allow SessionStorage a chance to register the closed window.
  yield new Promise(resolve => setTimeout(resolve, 20));
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
  is(SessionStore.lastClosedObjectType, "tab", "The last closed object is a tab");

  // Record the first closedId created.
  let initialClosedId = SessionStore.getClosedTabData(win, false)[0].closedId;

  // Open and close another window.
  let win2 = yield openWindow("about:mozilla");
  yield closeWindow(win2);  // closedId == initialClosedId + 1
  is(SessionStore.lastClosedObjectType, "window", "The last closed object is a window");

  // Open and close another tab in the first window.
  yield openAndCloseTab(win, "about:robots");  // closedId == initialClosedId + 2
  is(SessionStore.lastClosedObjectType, "tab", "The last closed object is a tab");

  // Undo closing the second tab.
  let tab = SessionStore.undoCloseById(initialClosedId + 2);
  yield promiseBrowserLoaded(tab.linkedBrowser);
  is(tab.linkedBrowser.currentURI.spec, "about:robots", "The expected tab was re-opened");

  let notTab = SessionStore.undoCloseById(initialClosedId + 2);
  is(notTab, undefined, "Re-opened tab cannot be unClosed again by closedId");

  // Now the last closed object should be a window again.
  is(SessionStore.lastClosedObjectType, "window", "The last closed object is a window");

  // Undo closing the first tab.
  let tab2 = SessionStore.undoCloseById(initialClosedId);
  yield promiseBrowserLoaded(tab2.linkedBrowser);
  is(tab2.linkedBrowser.currentURI.spec, "about:mozilla", "The expected tab was re-opened");

  // Close the two tabs we re-opened.
  yield promiseRemoveTab(tab); // closedId == initialClosedId + 3
  is(SessionStore.lastClosedObjectType, "tab", "The last closed object is a tab");
  yield promiseRemoveTab(tab2); // closedId == initialClosedId + 4
  is(SessionStore.lastClosedObjectType, "tab", "The last closed object is a tab");

  // Open another new window.
  let win3 = yield openWindow("about:mozilla");

  // Close both windows.
  yield closeWindow(win); // closedId == initialClosedId + 5
  is(SessionStore.lastClosedObjectType, "window", "The last closed object is a window");
  yield closeWindow(win3); // closedId == initialClosedId + 6
  is(SessionStore.lastClosedObjectType, "window", "The last closed object is a window");

  // Undo closing the second window.
  win = SessionStore.undoCloseById(initialClosedId + 6);
  yield BrowserTestUtils.waitForEvent(win, "load");

  // Make sure we wait until this window is restored.
  yield BrowserTestUtils.waitForEvent(win.gBrowser.tabContainer,
                                      "SSTabRestored");

  is(win.gBrowser.selectedBrowser.currentURI.spec, "about:mozilla", "The expected window was re-opened");

  let notWin = SessionStore.undoCloseById(initialClosedId + 6);
  is(notWin, undefined, "Re-opened window cannot be unClosed again by closedId");

  // Close the window again.
  yield closeWindow(win);
  is(SessionStore.lastClosedObjectType, "window", "The last closed object is a window");

  // Undo closing the first window.
  win = SessionStore.undoCloseById(initialClosedId + 5);

  yield BrowserTestUtils.waitForEvent(win, "load");

  // Make sure we wait until this window is restored.
  yield BrowserTestUtils.waitForEvent(win.gBrowser.tabContainer,
                                      "SSTabRestored");

  is(win.gBrowser.selectedBrowser.currentURI.spec, "about:robots", "The expected window was re-opened");

  // Close the window again.
  yield closeWindow(win);
  is(SessionStore.lastClosedObjectType, "window", "The last closed object is a window");
});
