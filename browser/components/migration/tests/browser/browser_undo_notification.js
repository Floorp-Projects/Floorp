"use strict";

let scope = {};
Cu.import("resource:///modules/AutoMigrate.jsm", scope);
let oldCanUndo = scope.AutoMigrate.canUndo;
let oldUndo = scope.AutoMigrate.undo;
registerCleanupFunction(function() {
  Cu.reportError("Cleaning up");
  scope.AutoMigrate.canUndo = oldCanUndo;
  scope.AutoMigrate.undo = oldUndo;
  Cu.reportError("Cleaned up");
});

const kExpectedNotificationId = "automigration-undo";

add_task(function* autoMigrationUndoNotificationShows() {
  let getNotification = browser =>
    gBrowser.getNotificationBox(browser).getNotificationWithValue(kExpectedNotificationId);

  scope.AutoMigrate.canUndo = () => true;
  let undoCalled;
  scope.AutoMigrate.undo = () => { undoCalled = true };
  for (let url of ["about:newtab", "about:home"]) {
    undoCalled = false;
    // Can't use pushPrefEnv because of bug 1323779
    Services.prefs.setCharPref("browser.migrate.automigrate.browser", "someunknownbrowser");
    let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, url, false);
    let browser = tab.linkedBrowser;
    if (!getNotification(browser)) {
      info(`Notification for ${url} not immediately present, waiting for it.`);
      yield BrowserTestUtils.waitForNotificationBar(gBrowser, browser, kExpectedNotificationId);
    }

    ok(true, `Got notification for ${url}`);
    let notification = getNotification(browser);
    let notificationBox = notification.parentNode;
    notification.querySelector("button.notification-button-default").click();
    ok(!undoCalled, "Undo should not be called when clicking the default button");
    is(notification, notificationBox._closedNotification, "Notification should be closing");
    yield BrowserTestUtils.removeTab(tab);

    undoCalled = false;
    Services.prefs.setCharPref("browser.migrate.automigrate.browser", "someunknownbrowser");
    tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, url, false);
    browser = tab.linkedBrowser;
    if (!getNotification(browser)) {
      info(`Notification for ${url} not immediately present, waiting for it.`);
      yield BrowserTestUtils.waitForNotificationBar(gBrowser, browser, kExpectedNotificationId);
    }

    ok(true, `Got notification for ${url}`);
    notification = getNotification(browser);
    notificationBox = notification.parentNode;
    notification.querySelector("button:not(.notification-button-default)").click();
    ok(undoCalled, "Undo should be called when clicking the non-default (Don't Keep) button");
    is(notification, notificationBox._closedNotification, "Notification should be closing");
    yield BrowserTestUtils.removeTab(tab);
  }
});

