"use strict";

add_task(async function autoMigrationUndoNotificationShows() {
  scope.AutoMigrate.canUndo = () => true;
  let undoCalled;
  scope.AutoMigrate.undo = () => { undoCalled = true; };

  for (let url of ["about:newtab", "about:home"]) {
    undoCalled = false;
    // Can't use pushPrefEnv because of bug 1323779
    Services.prefs.setCharPref("browser.migrate.automigrate.browser", "someunknownbrowser");
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url, false);
    let browser = tab.linkedBrowser;
    let notification = await getOrWaitForNotification(browser, url);
    ok(true, `Got notification for ${url}`);
    let notificationBox = notification.parentNode;
    notification.querySelector("button.notification-button-default").click();
    ok(!undoCalled, "Undo should not be called when clicking the default button");
    is(notification, notificationBox._closedNotification, "Notification should be closing");
    BrowserTestUtils.removeTab(tab);

    undoCalled = false;
    Services.prefs.setCharPref("browser.migrate.automigrate.browser", "chrome");
    tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url, false);
    browser = tab.linkedBrowser;
    notification = await getOrWaitForNotification(browser, url);
    ok(true, `Got notification for ${url}`);
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
    BrowserTestUtils.removeTab(surveyTab);
    BrowserTestUtils.removeTab(tab);
  }
});

