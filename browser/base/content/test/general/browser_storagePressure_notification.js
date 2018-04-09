/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

function notifyStoragePressure(usage = 100) {
  let notifyPromise = TestUtils.topicObserved("QuotaManager::StoragePressure", () => true);
  let usageWrapper = Cc["@mozilla.org/supports-PRUint64;1"]
                     .createInstance(Ci.nsISupportsPRUint64);
  usageWrapper.data = usage;
  Services.obs.notifyObservers(usageWrapper, "QuotaManager::StoragePressure");
  return notifyPromise;
}

function openAboutPrefPromise() {
  let promises = [
    BrowserTestUtils.waitForLocationChange(gBrowser, "about:preferences#privacy"),
    TestUtils.topicObserved("privacy-pane-loaded", () => true)
  ];
  return Promise.all(promises);
}

// Test only displaying notification once within the given interval
add_task(async function() {
  const TEST_NOTIFICATION_INTERVAL_MS = 2000;
  await SpecialPowers.pushPrefEnv({set: [["browser.storageManager.pressureNotification.minIntervalMS", TEST_NOTIFICATION_INTERVAL_MS]]});
  // Commenting this to see if we really need it
  // await SpecialPowers.pushPrefEnv({set: [["privacy.reduceTimerPrecision", false]]});

  await notifyStoragePressure();
  let notificationbox = document.getElementById("high-priority-global-notificationbox");
  let notification = notificationbox.getNotificationWithValue("storage-pressure-notification");
  ok(notification instanceof XULElement, "Should display storage pressure notification");
  notification.close();

  await notifyStoragePressure();
  notification = notificationbox.getNotificationWithValue("storage-pressure-notification");
  is(notification, null, "Should not display storage pressure notification more than once within the given interval");

  await new Promise(resolve => setTimeout(resolve, TEST_NOTIFICATION_INTERVAL_MS + 1));
  await notifyStoragePressure();
  notification = notificationbox.getNotificationWithValue("storage-pressure-notification");
  ok(notification instanceof XULElement, "Should display storage pressure notification after the given interval");
  notification.close();
});

// Test guiding user to the about:preferences when usage exceeds the given threshold
add_task(async function() {
  await SpecialPowers.pushPrefEnv({ set: [["browser.storageManager.pressureNotification.minIntervalMS", 0]] });
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "https://example.com");

  const BYTES_IN_GIGABYTE = 1073741824;
  const USAGE_THRESHOLD_BYTES = BYTES_IN_GIGABYTE *
    Services.prefs.getIntPref("browser.storageManager.pressureNotification.usageThresholdGB");
  await notifyStoragePressure(USAGE_THRESHOLD_BYTES);
  let notificationbox = document.getElementById("high-priority-global-notificationbox");
  let notification = notificationbox.getNotificationWithValue("storage-pressure-notification");
  ok(notification instanceof XULElement, "Should display storage pressure notification");

  let prefBtn = notification.getElementsByTagName("button")[1];
  let aboutPrefPromise = openAboutPrefPromise();
  prefBtn.doCommand();
  await aboutPrefPromise;
  let aboutPrefTab = gBrowser.selectedTab;
  let prefDoc = gBrowser.selectedBrowser.contentDocument;
  let siteDataGroup = prefDoc.getElementById("siteDataGroup");
  is_element_visible(siteDataGroup, "Should open to the siteDataGroup section in about:preferences");
  BrowserTestUtils.removeTab(aboutPrefTab);
  BrowserTestUtils.removeTab(tab);
});

// Test not displaying the 2nd notification if one is already being displayed
add_task(async function() {
  const TEST_NOTIFICATION_INTERVAL_MS = 0;
  await SpecialPowers.pushPrefEnv({set: [["browser.storageManager.pressureNotification.minIntervalMS", TEST_NOTIFICATION_INTERVAL_MS]]});

  await notifyStoragePressure();
  await notifyStoragePressure();
  let notificationbox = document.getElementById("high-priority-global-notificationbox");
  let allNotifications = notificationbox.allNotifications;
  let pressureNotificationCount = 0;
  allNotifications.forEach(notification => {
    if (notification.getAttribute("value") == "storage-pressure-notification") {
      pressureNotificationCount++;
    }
  });
  is(pressureNotificationCount, 1, "Should not display the 2nd notification when there is already one");
  notificationbox.removeAllNotifications();
});
