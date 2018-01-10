/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import("resource:///modules/SitePermissions.jsm");

const { SiteDataManager } = ChromeUtils.import("resource:///modules/SiteDataManager.jsm", {});
const { DownloadUtils } = ChromeUtils.import("resource://gre/modules/DownloadUtils.jsm", {});

const TEST_QUOTA_USAGE_HOST = "example.com";
const TEST_QUOTA_USAGE_ORIGIN = "https://" + TEST_QUOTA_USAGE_HOST;
const TEST_QUOTA_USAGE_URL = TEST_QUOTA_USAGE_ORIGIN + "/browser/browser/components/preferences/in-content/tests/site_data_test.html";
const TEST_OFFLINE_HOST = "example.org";
const TEST_OFFLINE_ORIGIN = "https://" + TEST_OFFLINE_HOST;
const TEST_OFFLINE_URL = TEST_OFFLINE_ORIGIN + "/browser/browser/components/preferences/in-content/tests/offline/offline.html";
const TEST_SERVICE_WORKER_URL = TEST_OFFLINE_ORIGIN + "/browser/browser/components/preferences/in-content/tests/service_worker_test.html";

// XXX: The intermittent bug 1331851
// The implementation of nsICacheStorageConsumptionObserver must be passed as weak referenced,
// so we must hold this observer here well. If we didn't, there would be a chance that
// in Linux debug test run the observer was released before the operation at gecko was completed
// (may be because of a relatively quicker GC cycle or a relatively slower operation).
// As a result of that, we would never get the cache usage we want so the test would fail from timeout.
const cacheUsageGetter = {
  _promise: null,
  _resolve: null,
  get() {
    if (!this._promise) {
      this._promise = new Promise(resolve => {
        this._resolve = resolve;
        Services.cache2.asyncGetDiskConsumption(this);
      });
    }
    return this._promise;
  },
  // nsICacheStorageConsumptionObserver implementations
  onNetworkCacheDiskConsumption(usage) {
    cacheUsageGetter._promise = null;
    cacheUsageGetter._resolve(usage);
  },
  QueryInterface: XPCOMUtils.generateQI([
    Components.interfaces.nsICacheStorageConsumptionObserver,
    Components.interfaces.nsISupportsWeakReference
  ]),
};

async function testClearData(clearSiteData, clearCache) {
  let quotaURI = Services.io.newURI(TEST_QUOTA_USAGE_ORIGIN);
  SitePermissions.set(quotaURI, "persistent-storage", SitePermissions.ALLOW);

  // Open a test site which saves into appcache.
  await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_OFFLINE_URL);
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);

  // Fill indexedDB with test data.
  // Don't wait for the page to load, to register the content event handler as quickly as possible.
  // If this test goes intermittent, we might have to tell the page to wait longer before
  // firing the event.
  BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_QUOTA_USAGE_URL, false);
  await BrowserTestUtils.waitForContentEvent(
    gBrowser.selectedBrowser, "test-indexedDB-done", false, null, true);
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);

  // Register some service workers.
  await loadServiceWorkerTestPage(TEST_SERVICE_WORKER_URL);
  await promiseServiceWorkerRegisteredFor(TEST_SERVICE_WORKER_URL);

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });

  // Test the initial states.
  let cacheUsage = await cacheUsageGetter.get();
  let quotaUsage = await getQuotaUsage(TEST_QUOTA_USAGE_ORIGIN);
  let totalUsage = await SiteDataManager.getTotalUsage();
  Assert.greater(cacheUsage, 0, "The cache usage should not be 0");
  Assert.greater(quotaUsage, 0, "The quota usage should not be 0");
  Assert.greater(totalUsage, 0, "The total usage should not be 0");

  let doc = gBrowser.selectedBrowser.contentDocument;
  let clearSiteDataButton = doc.getElementById("clearSiteDataButton");

  let dialogOpened = promiseLoadSubDialog("chrome://browser/content/preferences/clearSiteData.xul");
  clearSiteDataButton.doCommand();
  let dialogWin = await dialogOpened;

  // Convert the usage numbers in the same way the UI does it to assert
  // that they're displayed in the dialog.
  let [convertedTotalUsage] = DownloadUtils.convertByteUnits(totalUsage);
  // For cache we just assert that the right unit (KB, probably) is displayed,
  // since we've had cache intermittently changing under our feet.
  let [, convertedCacheUnit] = DownloadUtils.convertByteUnits(cacheUsage);

  let clearSiteDataLabel = dialogWin.document.getElementById("clearSiteDataLabel");
  let clearCacheLabel = dialogWin.document.getElementById("clearCacheLabel");
  // The usage details are filled asynchronously, so we assert that they're present by
  // waiting for them to be filled in.
  await Promise.all([
    TestUtils.waitForCondition(
      () => clearSiteDataLabel.value && clearSiteDataLabel.value.includes(convertedTotalUsage), "Should show the quota usage"),
    TestUtils.waitForCondition(
      () => clearCacheLabel.value && clearCacheLabel.value.includes(convertedCacheUnit), "Should show the cache usage")
  ]);

  // Check the boxes according to our test input.
  let clearSiteDataCheckbox = dialogWin.document.getElementById("clearSiteData");
  let clearCacheCheckbox = dialogWin.document.getElementById("clearCache");
  clearSiteDataCheckbox.checked = clearSiteData;
  clearCacheCheckbox.checked = clearCache;

  // Some additional promises/assertions to wait for
  // when deleting site data.
  let acceptPromise;
  let updatePromise;
  let cookiesClearedPromise;
  if (clearSiteData) {
    acceptPromise = promiseAlertDialogOpen("accept");
    updatePromise = promiseSiteDataManagerSitesUpdated();
    cookiesClearedPromise = promiseCookiesCleared();
  }

  let dialogClosed = BrowserTestUtils.waitForEvent(dialogWin, "unload");

  let clearButton = dialogWin.document.getElementById("clearButton");
  if (!clearSiteData && !clearCache) {
    // Simulate user input on one of the checkboxes to trigger the event listener for
    // disabling the clearButton.
    clearCacheCheckbox.doCommand();
    // Check that the clearButton gets disabled by unchecking both options.
    await TestUtils.waitForCondition(() => clearButton.disabled, "Clear button should be disabled");
    let cancelButton = dialogWin.document.getElementById("cancelButton");
    // Cancel, since we can't delete anything.
    cancelButton.click();
  } else {
    // Delete stuff!
    clearButton.click();
  }

  // For site data we display an extra warning dialog, make sure
  // to accept it.
  if (clearSiteData) {
    await acceptPromise;
  }

  await dialogClosed;

  if (clearCache) {
    TestUtils.waitForCondition(async function() {
      let usage = await cacheUsageGetter.get();
      return usage == 0;
    }, "The cache usage should be removed");
  } else {
    Assert.greater(await cacheUsageGetter.get(), 0, "The cache usage should not be 0");
  }

  if (clearSiteData) {
    await updatePromise;
    await cookiesClearedPromise;
    await promiseServiceWorkersCleared();

    TestUtils.waitForCondition(async function() {
      let usage = await SiteDataManager.getTotalUsage();
      return usage == 0;
    }, "The total usage should be removed");

    // Check that the size label in about:preferences updates after we cleared data.
    await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
      let sizeLabel = content.document.getElementById("totalSiteDataSize");

      await ContentTaskUtils.waitForCondition(
        () => sizeLabel.textContent.includes("0"), "Site data size label should have updated.");
    });
  } else {
    quotaUsage = await getQuotaUsage(TEST_QUOTA_USAGE_ORIGIN);
    totalUsage = await SiteDataManager.getTotalUsage();
    Assert.greater(quotaUsage, 0, "The quota usage should not be 0");
    Assert.greater(totalUsage, 0, "The total usage should not be 0");
  }

  let desiredPermissionState = clearSiteData ? SitePermissions.UNKNOWN : SitePermissions.ALLOW;
  let permission = SitePermissions.get(quotaURI, "persistent-storage");
  is(permission.state, desiredPermissionState, "Should have the correct permission state.");

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await SiteDataManager.removeAll();
}

// Test opening the "Clear All Data" dialog and cancelling.
add_task(async function() {
  await testClearData(false, false);
});

// Test opening the "Clear All Data" dialog and removing all site data.
add_task(async function() {
  await testClearData(true, false);
});

// Test opening the "Clear All Data" dialog and removing all cache.
add_task(async function() {
  await testClearData(false, true);
});

// Test opening the "Clear All Data" dialog and removing everything.
add_task(async function() {
  await testClearData(true, true);
});
