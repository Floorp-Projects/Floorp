"use strict";

/**
 * This test is for the sessionstore-closed-objects-changed notifications.
 */

requestLongerTimeout(2);

const MAX_WINDOWS_UNDO_PREF = "browser.sessionstore.max_windows_undo";
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
  // Create a closed window so that when we do the purge we know to expect a notification
  yield openAndCloseWindow("about:robots");

  // Forget any previous closed windows or tabs from other tests that may have
  // run in the same session.
  yield awaitNotification(() => Services.obs.notifyObservers(null, "browser:purge-session-history", 0));

  // Add an observer to count the number of notifications.
  Services.obs.addObserver(countingObserver, TOPIC);

  info("Opening and closing initial window.");
  yield openAndCloseWindow("about:robots");
  assertNotificationCount(1);

  // Store state with a single closed window for use in later tests.
  let closedState = Cu.cloneInto(JSON.parse(ss.getBrowserState()), {});

  info("Undoing close of initial window.");
  let win = SessionStore.undoCloseWindow(0);
  yield promiseDelayedStartupFinished(win);
  assertNotificationCount(2);

  // Open a second window.
  let win2 = yield openWindow("about:mozilla");

  info("Closing both windows.");
  yield closeWindow(win);
  assertNotificationCount(3);
  yield closeWindow(win2);
  assertNotificationCount(4);

  info(`Changing the ${MAX_WINDOWS_UNDO_PREF} pref.`);
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref(MAX_WINDOWS_UNDO_PREF);
  });
  yield awaitNotification(() => Services.prefs.setIntPref(MAX_WINDOWS_UNDO_PREF, 1));
  assertNotificationCount(5);

  info("Forgetting a closed window.");
  yield awaitNotification(() => SessionStore.forgetClosedWindow());
  assertNotificationCount(6);

  info("Opening and closing another window.");
  yield openAndCloseWindow("about:robots");
  assertNotificationCount(7);

  info("Setting browser state to trigger change onIdleDaily.")
  let state = Cu.cloneInto(JSON.parse(ss.getBrowserState()), {});
  state._closedWindows[0].closedAt = 1;
  yield promiseBrowserState(state);
  assertNotificationCount(8);

  info("Sending idle-daily");
  yield awaitNotification(() => Services.obs.notifyObservers(null, "idle-daily", ""));
  assertNotificationCount(9);

  info("Opening and closing another window.");
  yield openAndCloseWindow("about:robots");
  assertNotificationCount(10);

  info("Purging session history.");
  yield awaitNotification(() => Services.obs.notifyObservers(null, "browser:purge-session-history", 0));
  assertNotificationCount(11);

  info("Setting window state.")
  win = yield openWindow("about:mozilla");
  yield awaitNotification(() => SessionStore.setWindowState(win, closedState));
  assertNotificationCount(12);

  Services.obs.removeObserver(countingObserver, TOPIC);
  yield BrowserTestUtils.closeWindow(win);
});
