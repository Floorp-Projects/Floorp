/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This tests the new clear history dialog's data size display functionality
 */
/**
 * Helper function to validate the data sizes shown for each time selection
 *
 * @param {ClearHistoryDialogHelper} dh - dialog object to access window and timespan
 */
async function validateDataSizes(ClearHistoryDialogHelper) {
  let timespans = [
    "TIMESPAN_HOUR",
    "TIMESPAN_2HOURS",
    "TIMESPAN_4HOURS",
    "TIMESPAN_TODAY",
    "TIMESPAN_EVERYTHING",
  ];

  // get current data sizes from siteDataManager
  let cacheUsage = await SiteDataManager.getCacheSize();
  let quotaUsage = await SiteDataManager.getQuotaUsageForTimeRanges(timespans);

  for (let i = 0; i < timespans.length; i++) {
    // select timespan to check
    ClearHistoryDialogHelper.selectDuration(Sanitizer[timespans[i]]);

    // get the elements
    let clearCookiesAndSiteDataCheckbox =
      ClearHistoryDialogHelper.win.document.getElementById("cookiesAndStorage");
    let clearCacheCheckbox =
      ClearHistoryDialogHelper.win.document.getElementById("cache");

    let [convertedQuotaUsage] = DownloadUtils.convertByteUnits(
      quotaUsage[timespans[i]]
    );
    let [, convertedCacheUnit] = DownloadUtils.convertByteUnits(cacheUsage);

    // Ensure l10n is finished before inspecting the category labels.
    await ClearHistoryDialogHelper.win.document.l10n.translateElements([
      clearCookiesAndSiteDataCheckbox,
      clearCacheCheckbox,
    ]);
    ok(
      clearCacheCheckbox.label.includes(convertedCacheUnit),
      "Should show the cache usage"
    );
    ok(
      clearCookiesAndSiteDataCheckbox.label.includes(convertedQuotaUsage),
      `Should show the quota usage as ${convertedQuotaUsage}`
    );
  }
}

/**
 * Helper function to simulate switching timespan selections and
 * validate data sizes before and after clearing
 *
 * @param {Object}
 *    clearCookies - boolean
 *    clearDownloads - boolean
 *    clearCaches - boolean
 *    timespan - one of Sanitizer.TIMESPAN_*
 */
async function clearAndValidateDataSizes({
  clearCache,
  clearDownloads,
  clearCookies,
  timespan,
}) {
  await blankSlate();

  await addToDownloadList();
  await addToSiteUsage();
  let promiseSanitized = promiseSanitizationComplete();

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });

  let dh = new ClearHistoryDialogHelper();
  dh.onload = async function () {
    await validateDataSizes(this);
    this.checkPrefCheckbox("cache", clearCache);
    this.checkPrefCheckbox("cookiesAndStorage", clearCookies);
    this.checkPrefCheckbox("historyFormDataAndDownloads", clearDownloads);
    this.selectDuration(timespan);
    this.acceptDialog();
  };
  dh.onunload = async function () {
    await promiseSanitized;
  };
  dh.open();
  await dh.promiseClosed;

  let dh2 = new ClearHistoryDialogHelper();
  // Check if the newly cleared values are reflected
  dh2.onload = async function () {
    await validateDataSizes(this);
    this.acceptDialog();
  };
  dh2.open();
  await dh2.promiseClosed;

  await SiteDataTestUtils.clear();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
}

add_task(async function test_cookie_sizes() {
  await clearAndValidateDataSizes({
    clearCookies: true,
    clearCache: false,
    clearDownloads: false,
    timespan: Sanitizer.TIMESPAN_HOUR,
  });
  await clearAndValidateDataSizes({
    clearCookies: true,
    clearCache: false,
    clearDownloads: false,
    timespan: Sanitizer.TIMESPAN_4HOURS,
  });
  await clearAndValidateDataSizes({
    clearCookies: true,
    clearCache: false,
    clearDownloads: false,
    timespan: Sanitizer.TIMESPAN_EVERYTHING,
  });
});

add_task(async function test_cache_sizes() {
  await clearAndValidateDataSizes({
    clearCookies: false,
    clearCache: true,
    clearDownloads: false,
    timespan: Sanitizer.TIMESPAN_HOUR,
  });
  await clearAndValidateDataSizes({
    clearCookies: false,
    clearCache: true,
    clearDownloads: false,
    timespan: Sanitizer.TIMESPAN_4HOURS,
  });
  await clearAndValidateDataSizes({
    clearCookies: false,
    clearCache: true,
    clearDownloads: false,
    timespan: Sanitizer.TIMESPAN_EVERYTHING,
  });
});

add_task(async function test_all_data_sizes() {
  await clearAndValidateDataSizes({
    clearCookies: true,
    clearCache: true,
    clearDownloads: true,
    timespan: Sanitizer.TIMESPAN_HOUR,
  });
  await clearAndValidateDataSizes({
    clearCookies: true,
    clearCache: true,
    clearDownloads: true,
    timespan: Sanitizer.TIMESPAN_4HOURS,
  });
  await clearAndValidateDataSizes({
    clearCookies: true,
    clearCache: true,
    clearDownloads: true,
    timespan: Sanitizer.TIMESPAN_EVERYTHING,
  });
});
