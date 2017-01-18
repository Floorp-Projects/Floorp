"use strict";


const kExpectedNotificationId = "automigration-undo";

/**
 * Pretend we can undo something, trigger a notification, pick the undo option,
 * and verify that the notifications are all dismissed immediately.
 */
add_task(function* checkNotificationsDismissed() {
  yield SpecialPowers.pushPrefEnv({set: [
    ["browser.migrate.automigrate.enabled", true],
    ["browser.migrate.automigrate.ui.enabled", true],
  ]});
  let getNotification = browser =>
    gBrowser.getNotificationBox(browser).getNotificationWithValue(kExpectedNotificationId);

  Services.prefs.setCharPref("browser.migrate.automigrate.browser", "someunknownbrowser");

  let {guid, lastModified} = yield PlacesUtils.bookmarks.insert(
    {title: "Some imported bookmark", parentGuid: PlacesUtils.bookmarks.toolbarGuid, url: "http://www.example.com"}
  );

  let testUndoData = {
    visits: [],
    bookmarks: [{guid, lastModified: lastModified.getTime()}],
    logins: [],
  };
  let path = OS.Path.join(OS.Constants.Path.profileDir, "initialMigrationMetadata.jsonlz4");
  registerCleanupFunction(() => {
    return OS.File.remove(path, {ignoreAbsent: true});
  });
  yield OS.File.writeAtomic(path, JSON.stringify(testUndoData), {
    encoding: "utf-8",
    compression: "lz4",
    tmpPath: path + ".tmp",
  });

  let firstTab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:home", false);
  if (!getNotification(firstTab.linkedBrowser)) {
    info(`Notification not immediately present on first tab, waiting for it.`);
    yield BrowserTestUtils.waitForNotificationBar(gBrowser, firstTab.linkedBrowser, kExpectedNotificationId);
  }
  let secondTab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:home", false);
  if (!getNotification(secondTab.linkedBrowser)) {
    info(`Notification not immediately present on second tab, waiting for it.`);
    yield BrowserTestUtils.waitForNotificationBar(gBrowser, secondTab.linkedBrowser, kExpectedNotificationId);
  }

  // Create a listener for the removal in the first tab, and a listener for bookmarks removal,
  // then click 'Don't keep' in the second tab, and verify that the notification is removed
  // before we start removing bookmarks.
  let haveRemovedBookmark = false;
  let bmObserver;
  let bookmarkRemovedPromise = new Promise(resolve => {
    bmObserver = {
      onItemRemoved(itemId, parentId, index, itemType, uri, removedGuid) {
        if (guid == removedGuid) {
          haveRemovedBookmark = true;
          resolve();
        }
      },
    };
    PlacesUtils.bookmarks.addObserver(bmObserver, false);
    registerCleanupFunction(() => PlacesUtils.bookmarks.removeObserver(bmObserver));
  });

  let firstTabNotificationRemovedPromise = new Promise(resolve => {
    let notification = getNotification(firstTab.linkedBrowser);
    // Save this reference because notification.parentNode will be null once it's removed.
    let notificationBox = notification.parentNode;
    let mut = new MutationObserver(mutations => {
      // Yucky, but we have to detect either the removal via animation (with marginTop)
      // or when the element is removed. We can't just detect the element being removed
      // because this happens asynchronously (after the animation) and so it'd race
      // with the rest of the undo happening.
      for (let mutation of mutations) {
        if (mutation.target == notification && mutation.attributeName == "style" &&
            parseInt(notification.style.marginTop, 10) < 0) {
          ok(!haveRemovedBookmark, "Should not have removed bookmark yet");
          mut.disconnect();
          resolve();
          return;
        }
        if (mutation.target == notificationBox && mutation.removedNodes.length &&
            mutation.removedNodes[0] == notification) {
          ok(!haveRemovedBookmark, "Should not have removed bookmark yet");
          mut.disconnect();
          resolve();
          return;
        }
      }
    });
    mut.observe(notification.parentNode, {childList: true});
    mut.observe(notification, {attributes: true});
  });

  let prefResetPromise = new Promise(resolve => {
    const kObservedPref = "browser.migrate.automigrate.browser";
    let obs = () => {
      Services.prefs.removeObserver(kObservedPref, obs);
      ok(!Services.prefs.prefHasUserValue(kObservedPref),
         "Pref should have been reset");
      resolve();
    };
    Services.prefs.addObserver(kObservedPref, obs, false);
  });

  // Click "Don't keep" button:
  let notificationToActivate = getNotification(secondTab.linkedBrowser);
  notificationToActivate.querySelector("button:not(.notification-button-default)").click();
  info("Waiting for notification to be removed in first (background) tab");
  yield firstTabNotificationRemovedPromise;
  info("Waiting for bookmark to be removed");
  yield bookmarkRemovedPromise;
  info("Waiting for prefs to be reset");
  yield prefResetPromise;

  info("Removing spare tabs");
  yield BrowserTestUtils.removeTab(firstTab);
  yield BrowserTestUtils.removeTab(secondTab);
});
