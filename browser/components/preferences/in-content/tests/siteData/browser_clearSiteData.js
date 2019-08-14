/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var { SitePermissions } = ChromeUtils.import(
  "resource:///modules/SitePermissions.jsm"
);
const { PermissionTestUtils } = ChromeUtils.import(
  "resource://testing-common/PermissionTestUtils.jsm"
);

async function testClearData(clearSiteData, clearCache) {
  PermissionTestUtils.add(
    TEST_QUOTA_USAGE_ORIGIN,
    "persistent-storage",
    Services.perms.ALLOW_ACTION
  );

  // Open a test site which saves into appcache.
  await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_OFFLINE_URL);
  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  // Fill indexedDB with test data.
  // Don't wait for the page to load, to register the content event handler as quickly as possible.
  // If this test goes intermittent, we might have to tell the page to wait longer before
  // firing the event.
  BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_QUOTA_USAGE_URL, false);
  await BrowserTestUtils.waitForContentEvent(
    gBrowser.selectedBrowser,
    "test-indexedDB-done",
    false,
    null,
    true
  );
  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  // Register some service workers.
  await loadServiceWorkerTestPage(TEST_SERVICE_WORKER_URL);
  await promiseServiceWorkerRegisteredFor(TEST_SERVICE_WORKER_URL);

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });

  // Test the initial states.
  let cacheUsage = await SiteDataManager.getCacheSize();
  let quotaUsage = await SiteDataTestUtils.getQuotaUsage(
    TEST_QUOTA_USAGE_ORIGIN
  );
  let totalUsage = await SiteDataManager.getTotalUsage();
  Assert.greater(cacheUsage, 0, "The cache usage should not be 0");
  Assert.greater(quotaUsage, 0, "The quota usage should not be 0");
  Assert.greater(totalUsage, 0, "The total usage should not be 0");

  let initialSizeLabelValue = await ContentTask.spawn(
    gBrowser.selectedBrowser,
    null,
    async function() {
      let sizeLabel = content.document.getElementById("totalSiteDataSize");
      return sizeLabel.textContent;
    }
  );

  let doc = gBrowser.selectedBrowser.contentDocument;
  let clearSiteDataButton = doc.getElementById("clearSiteDataButton");

  let dialogOpened = promiseLoadSubDialog(
    "chrome://browser/content/preferences/clearSiteData.xul"
  );
  clearSiteDataButton.doCommand();
  let dialogWin = await dialogOpened;

  // Convert the usage numbers in the same way the UI does it to assert
  // that they're displayed in the dialog.
  let [convertedTotalUsage] = DownloadUtils.convertByteUnits(totalUsage);
  // For cache we just assert that the right unit (KB, probably) is displayed,
  // since we've had cache intermittently changing under our feet.
  let [, convertedCacheUnit] = DownloadUtils.convertByteUnits(cacheUsage);

  let clearSiteDataCheckbox = dialogWin.document.getElementById(
    "clearSiteData"
  );
  let clearCacheCheckbox = dialogWin.document.getElementById("clearCache");
  // The usage details are filled asynchronously, so we assert that they're present by
  // waiting for them to be filled in.
  await Promise.all([
    TestUtils.waitForCondition(
      () =>
        clearSiteDataCheckbox.label &&
        clearSiteDataCheckbox.label.includes(convertedTotalUsage),
      "Should show the quota usage"
    ),
    TestUtils.waitForCondition(
      () =>
        clearCacheCheckbox.label &&
        clearCacheCheckbox.label.includes(convertedCacheUnit),
      "Should show the cache usage"
    ),
  ]);

  // Check the boxes according to our test input.
  clearSiteDataCheckbox.checked = clearSiteData;
  clearCacheCheckbox.checked = clearCache;

  // Some additional promises/assertions to wait for
  // when deleting site data.
  let acceptPromise;
  let updatePromise;
  let cookiesClearedPromise;
  if (clearSiteData) {
    acceptPromise = BrowserTestUtils.promiseAlertDialogOpen("accept");
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
    await TestUtils.waitForCondition(
      () => clearButton.disabled,
      "Clear button should be disabled"
    );
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
      let usage = await SiteDataManager.getCacheSize();
      return usage == 0;
    }, "The cache usage should be removed");
  } else {
    Assert.greater(
      await SiteDataManager.getCacheSize(),
      0,
      "The cache usage should not be 0"
    );
  }

  if (clearSiteData) {
    await updatePromise;
    await cookiesClearedPromise;
    await promiseServiceWorkersCleared();

    TestUtils.waitForCondition(async function() {
      let usage = await SiteDataManager.getTotalUsage();
      return usage == 0;
    }, "The total usage should be removed");
  } else {
    quotaUsage = await SiteDataTestUtils.getQuotaUsage(TEST_QUOTA_USAGE_ORIGIN);
    totalUsage = await SiteDataManager.getTotalUsage();
    Assert.greater(quotaUsage, 0, "The quota usage should not be 0");
    Assert.greater(totalUsage, 0, "The total usage should not be 0");
  }

  if (clearCache || clearSiteData) {
    // Check that the size label in about:preferences updates after we cleared data.
    await ContentTask.spawn(
      gBrowser.selectedBrowser,
      { initialSizeLabelValue },
      async function(opts) {
        let sizeLabel = content.document.getElementById("totalSiteDataSize");
        await ContentTaskUtils.waitForCondition(
          () => sizeLabel.textContent != opts.initialSizeLabelValue,
          "Site data size label should have updated."
        );
      }
    );
  }

  let permission = PermissionTestUtils.getPermissionObject(
    TEST_QUOTA_USAGE_ORIGIN,
    "persistent-storage"
  );
  is(
    clearSiteData ? permission : permission.capability,
    clearSiteData ? null : Services.perms.ALLOW_ACTION,
    "Should have the correct permission state."
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
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
