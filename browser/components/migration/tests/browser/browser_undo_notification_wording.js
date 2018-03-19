"use strict";

add_task(async function autoMigrationUndoNotificationShows() {
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

    let notification = await getOrWaitForNotification(browser, url);
    ok(true, `Got notification for ${url}`);
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

    BrowserTestUtils.removeTab(tab);
  }
});

