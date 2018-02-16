"use strict";

/* exported getOrWaitForNotification */

// Import AutoMigrate and save/restore methods to allow stubbing
const scope = {};
ChromeUtils.import("resource:///modules/AutoMigrate.jsm", scope);
const oldCanUndo = scope.AutoMigrate.canUndo;
const oldUndo = scope.AutoMigrate.undo;
registerCleanupFunction(function() {
  scope.AutoMigrate.canUndo = oldCanUndo;
  scope.AutoMigrate.undo = oldUndo;
});

const kExpectedNotificationId = "automigration-undo";

/**
 * Helper to get the undo notification bar.
 */
function getNotification(browser) {
  const box = gBrowser.getNotificationBox(browser).getNotificationWithValue(kExpectedNotificationId);
  // Before activity stream, the page itself would trigger the notification bar.
  // Until bug 1438305 to decide on how to handle migration, keep integration
  // tests running by triggering the notification bar directly via tests.
  if (!box) {
    executeSoon(() => scope.AutoMigrate.showUndoNotificationBar(browser));
  }
  return box;
}

/**
 * Helper to get or wait for the undo notification bar.
 */
function getOrWaitForNotification(browser, description) {
  const notification = getNotification(browser);
  if (notification) {
    return notification;
  }

  info(`Notification for ${description} not immediately present, waiting for it.`);
  return BrowserTestUtils.waitForNotificationBar(gBrowser, browser, kExpectedNotificationId);
}
