"use strict";

let scope = {};
Cu.import("resource:///modules/AutoMigrate.jsm", scope);
let oldCanUndo = scope.AutoMigrate.canUndo;
let oldUndo = scope.AutoMigrate.undo;
registerCleanupFunction(function() {
  scope.AutoMigrate.canUndo = oldCanUndo;
  scope.AutoMigrate.undo = oldUndo;
});

const kExpectedNotificationId = "automigration-undo";

add_task(async function autoMigrationUndoNotificationShows() {
  let getNotification = browser =>
    gBrowser.getNotificationBox(browser).getNotificationWithValue(kExpectedNotificationId);

  scope.AutoMigrate.canUndo = () => true;
  let undoCalled;
  scope.AutoMigrate.undo = () => { undoCalled = true };

  // Disable preloaded activity-stream about:newtab for this test to only run on
  // the tiles version
  await SpecialPowers.pushPrefEnv({set: [
    ["browser.newtabpage.activity-stream.enabled", false],
    ["browser.newtab.preload", false]
  ]});
  // Open and close about:newtab to remove any preloaded activity-stream version
  await BrowserTestUtils.removeTab(await BrowserTestUtils.openNewForegroundTab(gBrowser));

  for (let url of ["about:newtab", "about:home"]) {
    undoCalled = false;
    // Can't use pushPrefEnv because of bug 1323779
    Services.prefs.setCharPref("browser.migrate.automigrate.browser", "someunknownbrowser");
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url, false);
    let browser = tab.linkedBrowser;
    if (!getNotification(browser)) {
      info(`Notification for ${url} not immediately present, waiting for it.`);
      await BrowserTestUtils.waitForNotificationBar(gBrowser, browser, kExpectedNotificationId);
    }

    ok(true, `Got notification for ${url}`);
    let notification = getNotification(browser);
    let notificationBox = notification.parentNode;
    notification.querySelector("button.notification-button-default").click();
    ok(!undoCalled, "Undo should not be called when clicking the default button");
    is(notification, notificationBox._closedNotification, "Notification should be closing");
    await BrowserTestUtils.removeTab(tab);

    undoCalled = false;
    Services.prefs.setCharPref("browser.migrate.automigrate.browser", "chrome");
    tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url, false);
    browser = tab.linkedBrowser;
    if (!getNotification(browser)) {
      info(`Notification for ${url} not immediately present, waiting for it.`);
      await BrowserTestUtils.waitForNotificationBar(gBrowser, browser, kExpectedNotificationId);
    }

    ok(true, `Got notification for ${url}`);
    notification = getNotification(browser);
    notificationBox = notification.parentNode;
    // Set up the survey:
    await SpecialPowers.pushPrefEnv({set: [
      ["browser.migrate.automigrate.undo-survey", "https://example.com/?browser=%IMPORTEDBROWSER%"],
      ["browser.migrate.automigrate.undo-survey-locales", "en-US"],
    ]});
    let tabOpenedPromise = BrowserTestUtils.waitForNewTab(gBrowser, "https://example.com/?browser=Google%20Chrome");
    notification.querySelector("button:not(.notification-button-default)").click();
    ok(undoCalled, "Undo should be called when clicking the non-default (Don't Keep) button");
    is(notification, notificationBox._closedNotification, "Notification should be closing");
    let surveyTab = await tabOpenedPromise;
    ok(surveyTab, "Should have opened a tab with a survey");
    await BrowserTestUtils.removeTab(surveyTab);
    await BrowserTestUtils.removeTab(tab);
  }
});

