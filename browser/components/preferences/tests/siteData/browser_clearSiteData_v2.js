/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PermissionTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PermissionTestUtils.sys.mjs"
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

  let initialSizeLabelValue = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    async function () {
      let sizeLabel = content.document.getElementById("totalSiteDataSize");
      return sizeLabel.textContent;
    }
  );

  let doc = gBrowser.selectedBrowser.contentDocument;
  let clearSiteDataButton = doc.getElementById("clearSiteDataButton");

  let url = "chrome://browser/content/sanitize_v2.xhtml";
  let dialogOpened = promiseLoadSubDialog(url);
  clearSiteDataButton.doCommand();
  let dialogWin = await dialogOpened;

  // Convert the usage numbers in the same way the UI does it to assert
  // that they're displayed in the dialog.
  let [convertedTotalUsage] = DownloadUtils.convertByteUnits(totalUsage);
  // For cache we just assert that the right unit (KB, probably) is displayed,
  // since we've had cache intermittently changing under our feet.
  let [, convertedCacheUnit] = DownloadUtils.convertByteUnits(cacheUsage);

  let cookiesCheckboxId = "cookiesAndStorage";
  let cacheCheckboxId = "cache";
  let clearSiteDataCheckbox =
    dialogWin.document.getElementById(cookiesCheckboxId);
  let clearCacheCheckbox = dialogWin.document.getElementById(cacheCheckboxId);
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

  // select clear everything to match the old dialog boxes behaviour for this test
  let timespanSelection = dialogWin.document.getElementById(
    "sanitizeDurationChoice"
  );
  timespanSelection.value = 1;

  // Some additional promises/assertions to wait for
  // when deleting site data.
  let updatePromise;
  if (clearSiteData) {
    // the new clear history dialog does not have a extra prompt
    // to clear site data after clicking clear
    updatePromise = promiseSiteDataManagerSitesUpdated();
  }

  let dialogClosed = BrowserTestUtils.waitForEvent(dialogWin, "unload");

  let clearButton = dialogWin.document
    .querySelector("dialog")
    .getButton("accept");
  let cancelButton = dialogWin.document
    .querySelector("dialog")
    .getButton("cancel");

  if (!clearSiteData && !clearCache) {
    // Cancel, since we can't delete anything.
    cancelButton.click();
  } else {
    // Delete stuff!
    clearButton.click();
  }

  await dialogClosed;

  if (clearCache) {
    TestUtils.waitForCondition(async function () {
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
    await promiseServiceWorkersCleared();

    TestUtils.waitForCondition(async function () {
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
    await SpecialPowers.spawn(
      gBrowser.selectedBrowser,
      [{ initialSizeLabelValue }],
      async function (opts) {
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

add_setup(function () {
  SpecialPowers.pushPrefEnv({
    set: [["privacy.sanitize.useOldClearHistoryDialog", false]],
  });

  // The tests in this file all test specific interactions with the new clear
  // history dialog and can't be split up.
  requestLongerTimeout(2);
});

// Test opening the "Clear All Data" dialog and cancelling.
add_task(async function testNoSiteDataNoCacheClearing() {
  await testClearData(false, false);
});

// Test opening the "Clear All Data" dialog and removing all site data.
add_task(async function testSiteDataClearing() {
  await testClearData(true, false);
});

// Test opening the "Clear All Data" dialog and removing all cache.
add_task(async function testCacheClearing() {
  await testClearData(false, true);
});

// Test opening the "Clear All Data" dialog and removing everything.
add_task(async function testSiteDataAndCacheClearing() {
  await testClearData(true, true);
});

// Test clearing persistent storage
add_task(async function testPersistentStorage() {
  PermissionTestUtils.add(
    TEST_QUOTA_USAGE_ORIGIN,
    "persistent-storage",
    Services.perms.ALLOW_ACTION
  );

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });

  let doc = gBrowser.selectedBrowser.contentDocument;
  let clearSiteDataButton = doc.getElementById("clearSiteDataButton");

  let url = "chrome://browser/content/sanitize_v2.xhtml";
  let dialogOpened = promiseLoadSubDialog(url);
  clearSiteDataButton.doCommand();
  let dialogWin = await dialogOpened;
  let dialogClosed = BrowserTestUtils.waitForEvent(dialogWin, "unload");

  let timespanSelection = dialogWin.document.getElementById(
    "sanitizeDurationChoice"
  );
  timespanSelection.value = 1;
  let clearButton = dialogWin.document
    .querySelector("dialog")
    .getButton("accept");
  clearButton.click();
  await dialogClosed;

  let permission = PermissionTestUtils.getPermissionObject(
    TEST_QUOTA_USAGE_ORIGIN,
    "persistent-storage"
  );
  is(permission, null, "Should have the correct permission state.");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
