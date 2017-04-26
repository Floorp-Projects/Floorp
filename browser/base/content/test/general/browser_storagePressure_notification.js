/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function notifyStoragePressure(usage = 100) {
  let notifyPromise = TestUtils.topicObserved("QuotaManager::StoragePressure", () => true);
  let usageWrapper = Cc["@mozilla.org/supports-PRUint64;1"]
                     .createInstance(Ci.nsISupportsPRUint64);
  usageWrapper.data = usage;
  Services.obs.notifyObservers(usageWrapper, "QuotaManager::StoragePressure");
  return notifyPromise;
}

function privacyAboutPrefPromise() {
  let promises = [
    BrowserTestUtils.waitForLocationChange(gBrowser, "about:preferences#privacy"),
    TestUtils.topicObserved("advanced-pane-loaded", () => true)
  ];
  return Promise.all(promises);
}

// Test only displaying notification once within the given interval
add_task(function* () {
  const TEST_NOTIFICATION_INTERVAL_MS = 2000;
  yield SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
  yield SpecialPowers.pushPrefEnv({set: [["browser.storageManager.pressureNotification.minIntervalMS", TEST_NOTIFICATION_INTERVAL_MS]]});

  yield notifyStoragePressure();
  let notificationbox = document.getElementById("high-priority-global-notificationbox");
  let notification = notificationbox.getNotificationWithValue("storage-pressure-notification");
  ok(notification instanceof XULElement, "Should display storage pressure notification");
  notification.close();

  yield notifyStoragePressure();
  notification = notificationbox.getNotificationWithValue("storage-pressure-notification");
  is(notification, null, "Should not display storage pressure notification more than once within the given interval");

  yield new Promise(resolve => setTimeout(resolve, TEST_NOTIFICATION_INTERVAL_MS + 1));
  yield notifyStoragePressure();
  notification = notificationbox.getNotificationWithValue("storage-pressure-notification");
  ok(notification instanceof XULElement, "Should display storage pressure notification after the given interval");
  notification.close();
});

// Test guiding user to about:preferences when usage exceeds the given threshold
add_task(function* () {
  yield SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
  yield SpecialPowers.pushPrefEnv({set: [["browser.storageManager.pressureNotification.minIntervalMS", 0]]});

  const BYTES_IN_GIGABYTE = 1073741824;
  const USAGE_THRESHOLD_BYTES = BYTES_IN_GIGABYTE *
    Services.prefs.getIntPref("browser.storageManager.pressureNotification.usageThresholdGB");
  yield notifyStoragePressure(USAGE_THRESHOLD_BYTES);
  let notificationbox = document.getElementById("high-priority-global-notificationbox");
  let notification = notificationbox.getNotificationWithValue("storage-pressure-notification");
  ok(notification instanceof XULElement, "Should display storage pressure notification");

  let prefBtn = notification.getElementsByTagName("button")[1];
  let aboutPrefPromise = privacyAboutPrefPromise();
  prefBtn.doCommand();
  yield aboutPrefPromise;
  let prefDoc = gBrowser.selectedBrowser.contentDocument;
  let siteDataGroup = prefDoc.getElementById("siteDataGroup");
  is_element_visible(siteDataGroup, "Should open the Network tab in about:preferences#privacy");
});
