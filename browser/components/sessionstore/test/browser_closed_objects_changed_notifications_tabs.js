"use strict";

/**
 * This test is for the sessionstore-closed-objects-changed notifications.
 */

const MAX_TABS_UNDO_PREF = "browser.sessionstore.max_tabs_undo";
const TOPIC = "sessionstore-closed-objects-changed";

let notificationsCount = 0;

function* openWindow(url) {
  let win = yield promiseNewWindowLoaded();
  let flags = Ci.nsIWebNavigation.LOAD_FLAGS_REPLACE_HISTORY;
  win.gBrowser.selectedBrowser.loadURIWithFlags(url, flags);
  yield promiseBrowserLoaded(win.gBrowser.selectedBrowser, true, url);
  return win;
}

function* closeWindow(win) {
  yield awaitNotification(() => BrowserTestUtils.closeWindow(win));
}

function* openAndCloseWindow(url) {
  let win = yield openWindow(url);
  yield closeWindow(win);
}

function* openTab(window, url) {
  let tab = yield BrowserTestUtils.openNewForegroundTab(window.gBrowser, url);
  yield TabStateFlusher.flush(tab.linkedBrowser);
  return tab;
}

function* openAndCloseTab(window, url) {
  let tab = yield openTab(window, url);
  yield promiseRemoveTab(tab);
}

function countingObserver() {
  notificationsCount++;
}

function assertNotificationCount(count) {
  is(notificationsCount, count, "The expected number of notifications was received.");
}

function* awaitNotification(callback) {
  let notification = TestUtils.topicObserved(TOPIC);
  executeSoon(callback);
  yield notification;
}

add_task(function* test_closedObjectsChangedNotifications() {
  // Create a closed window so that when we do the purge we can expect a notification.
  yield openAndCloseWindow("about:robots");

  // Forget any previous closed windows or tabs from other tests that may have
  // run in the same session.
  yield awaitNotification(() => Services.obs.notifyObservers(null, "browser:purge-session-history"));

  // Add an observer to count the number of notifications.
  Services.obs.addObserver(countingObserver, TOPIC);

  // Open a new window.
  let win = yield openWindow("about:robots");

  info("Opening and closing a tab.");
  yield openAndCloseTab(win, "about:mozilla");
  assertNotificationCount(1);

  info("Opening and closing a second tab.");
  yield openAndCloseTab(win, "about:mozilla");
  assertNotificationCount(2);

  info(`Changing the ${MAX_TABS_UNDO_PREF} pref.`);
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref(MAX_TABS_UNDO_PREF);
  });
  yield awaitNotification(() => Services.prefs.setIntPref(MAX_TABS_UNDO_PREF, 1));
  assertNotificationCount(3);

  info("Undoing close of remaining closed tab.");
  let tab = SessionStore.undoCloseTab(win, 0);
  yield promiseTabRestored(tab);
  assertNotificationCount(4);

  info("Closing tab again.");
  yield promiseRemoveTab(tab);
  assertNotificationCount(5);

  info("Purging session history.");
  yield awaitNotification(() => Services.obs.notifyObservers(null, "browser:purge-session-history"));
  assertNotificationCount(6);

  info("Opening and closing another tab.");
  yield openAndCloseTab(win, "http://example.com/");
  assertNotificationCount(7);

  info("Purging domain data with no matches.")
  Services.obs.notifyObservers(null, "browser:purge-domain-data", "mozilla.com");
  assertNotificationCount(7);

  info("Purging domain data with matches.")
  yield awaitNotification(() => Services.obs.notifyObservers(null, "browser:purge-domain-data", "example.com"));
  assertNotificationCount(8);

  info("Opening and closing another tab.");
  yield openAndCloseTab(win, "http://example.com/");
  assertNotificationCount(9);

  yield closeWindow(win);
  assertNotificationCount(10);

  Services.obs.removeObserver(countingObserver, TOPIC);
});
