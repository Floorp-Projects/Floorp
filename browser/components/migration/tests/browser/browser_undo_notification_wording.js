"use strict";

let scope = {};
Cu.import("resource:///modules/AutoMigrate.jsm", scope);
let oldCanUndo = scope.AutoMigrate.canUndo;
registerCleanupFunction(function() {
  scope.AutoMigrate.canUndo = oldCanUndo;
});

const kExpectedNotificationId = "automigration-undo";

add_task(async function autoMigrationUndoNotificationShows() {
  let getNotification = browser =>
    gBrowser.getNotificationBox(browser).getNotificationWithValue(kExpectedNotificationId);
  let localizedVersionOf = str => {
    if (str == "logins") {
      return "passwords";
    }
    if (str == "visits") {
      return "history";
    }
    return str;
  };

  scope.AutoMigrate.canUndo = () => true;
  let url = "about:home";
  Services.prefs.setCharPref("browser.migrate.automigrate.browser", "someunknownbrowser");
  const kSubsets = [
    ["bookmarks", "logins", "visits"],
    ["bookmarks", "logins"],
    ["bookmarks", "visits"],
    ["logins", "visits"],
    ["bookmarks"],
    ["logins"],
    ["visits"],
  ];
  const kAllItems = ["bookmarks", "logins", "visits"];
  for (let subset of kSubsets) {
    let state = new Map(subset.map(item => [item, [{}]]));
    scope.AutoMigrate._setImportedItemPrefFromState(state);
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url, false);
    let browser = tab.linkedBrowser;

    if (!getNotification(browser)) {
      info(`Notification for ${url} not immediately present, waiting for it.`);
      await BrowserTestUtils.waitForNotificationBar(gBrowser, browser, kExpectedNotificationId);
    }

    ok(true, `Got notification for ${url}`);
    let notification = getNotification(browser);
    let notificationText = document.getAnonymousElementByAttribute(notification, "class", "messageText");
    notificationText = notificationText.textContent;
    for (let potentiallyImported of kAllItems) {
      let localizedImportItem = localizedVersionOf(potentiallyImported);
      if (subset.includes(potentiallyImported)) {
        ok(notificationText.includes(localizedImportItem),
           "Expected notification to contain " + localizedImportItem);
      } else {
        ok(!notificationText.includes(localizedImportItem),
           "Expected notification not to contain " + localizedImportItem);
      }
    }

    await BrowserTestUtils.removeTab(tab);
  }
});

