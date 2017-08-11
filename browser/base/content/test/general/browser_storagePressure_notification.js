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

function openAboutPrefPromise() {
  let useOldOrganization = Services.prefs.getBoolPref("browser.preferences.useOldOrganization");
  let targetURL = useOldOrganization ? "about:preferences#advanced" : "about:preferences#privacy";
  let promises = [
    BrowserTestUtils.waitForLocationChange(gBrowser, targetURL),
    TestUtils.topicObserved("advanced-pane-loaded", () => true)
  ];
  return Promise.all(promises);
}

async function testOverUsageThresholdNotification() {
  await SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
  await SpecialPowers.pushPrefEnv({set: [["browser.storageManager.pressureNotification.minIntervalMS", 0]]});
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
  await BrowserTestUtils.removeTab(aboutPrefTab);
  await BrowserTestUtils.removeTab(tab);
}

// Test only displaying notification once within the given interval
add_task(async function() {
  const TEST_NOTIFICATION_INTERVAL_MS = 2000;
  await SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
  await SpecialPowers.pushPrefEnv({set: [["browser.storageManager.pressureNotification.minIntervalMS", TEST_NOTIFICATION_INTERVAL_MS]]});

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
  // Test for the old about:preferences
  await SpecialPowers.pushPrefEnv({set: [["browser.preferences.useOldOrganization", true]]});
  await testOverUsageThresholdNotification();
  // Test for the new about:preferences
  await SpecialPowers.pushPrefEnv({set: [["browser.preferences.useOldOrganization", false]]});
  await testOverUsageThresholdNotification();
});
