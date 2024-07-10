/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This tests the new clear history dialog's data size display functionality
 */
ChromeUtils.defineESModuleGetters(this, {
  sinon: "resource://testing-common/Sinon.sys.mjs",
  Sanitizer: "resource:///modules/Sanitizer.sys.mjs",
});

const LARGE_USAGE_NUM = 100000000000000000000000000000000000000000000000000;

function isIframeOverflowing(win) {
  return (
    win.scrollWidth > win.clientWidth || win.scrollHeight > win.clientHeight
  );
}

add_setup(async function () {
  await blankSlate();
  requestLongerTimeout(2);
  registerCleanupFunction(async function () {
    await blankSlate();
    await PlacesTestUtils.promiseAsyncUpdates();
    await SiteDataTestUtils.clear();
  });
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.sanitize.useOldClearHistoryDialog", false]],
  });
});

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

  await addToSiteUsage();
  let promiseSanitized = promiseSanitizationComplete();

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });

  let dh = new ClearHistoryDialogHelper({ checkingDataSizes: true });
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

  let dh2 = new ClearHistoryDialogHelper({ checkingDataSizes: true });
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

// This test makes sure that the user can change their timerange option
// even if the data sizes are not loaded yet.
add_task(async function testUIWithDataSizesLoading() {
  await blankSlate();
  await addToSiteUsage();

  let origGetQuotaUsageForTimeRanges =
    SiteDataManager.getQuotaUsageForTimeRanges.bind(SiteDataManager);
  let resolveStubFn;
  let resolverAssigned = false;

  let dh = new ClearHistoryDialogHelper();
  // Create a sandbox for isolated stubbing within the test
  let sandbox = sinon.createSandbox();
  sandbox
    .stub(SiteDataManager, "getQuotaUsageForTimeRanges")
    .callsFake(async (...args) => {
      info("stub called");

      let dataSizesReadyToLoadPromise = new Promise(resolve => {
        resolveStubFn = resolve;
        info("Sending message to notify dialog that the resolver is assigned");
        window.postMessage("resolver-assigned", "*");
        resolverAssigned = true;
      });
      await dataSizesReadyToLoadPromise;
      return origGetQuotaUsageForTimeRanges(...args);
    });
  dh.onload = async function () {
    // we add this event listener in the case where init finishes before the resolver is assigned
    if (!resolverAssigned) {
      await new Promise(resolve => {
        let listener = event => {
          if (event.data === "resolver-assigned") {
            window.removeEventListener("message", listener);
            // we are ready to test the dialog without any data sizes loaded
            resolve();
          }
        };
        window.addEventListener("message", listener);
      });
    }

    ok(
      !this.win.gSanitizePromptDialog._dataSizesUpdated,
      "Data sizes should not have loaded yet"
    );
    this.selectDuration(Sanitizer.TIMESPAN_2HOURS);

    info("triggering loading state end");
    resolveStubFn();

    await this.win.gSanitizePromptDialog.dataSizesFinishedUpdatingPromise;

    validateDataSizes(this);
    this.cancelDialog();
  };
  dh.open();
  await dh.promiseClosed;

  // Restore the sandbox after the test is complete
  sandbox.restore();
});

add_task(async function testClearingBeforeDataSizesLoad() {
  await blankSlate();
  await addToSiteUsage();

  // add site data that we can verify if it gets cleared
  await createDummyDataForHost("example.org");
  await createDummyDataForHost("example.com");

  ok(
    await SiteDataTestUtils.hasIndexedDB("https://example.org"),
    "We have indexedDB data for example.org"
  );
  ok(
    await SiteDataTestUtils.hasIndexedDB("https://example.com"),
    "We have indexedDB data for example.com"
  );

  let dh = new ClearHistoryDialogHelper();
  let promiseSanitized = promiseSanitizationComplete();
  // Create a sandbox for isolated stubbing within the test
  let sandbox = sinon.createSandbox();
  sandbox
    .stub(SiteDataManager, "getQuotaUsageForTimeRanges")
    .callsFake(async () => {
      info("stub called");

      info("This promise should never resolve");
      await new Promise(() => {});
    });
  dh.onload = async function () {
    // we don't need to initiate a event listener to wait for the resolver to be assigned for this
    // test since we do not want the data sizes to load
    ok(
      !this.win.gSanitizePromptDialog._dataSizesUpdated,
      "Data sizes should not be loaded yet"
    );
    this.selectDuration(Sanitizer.TIMESPAN_2HOURS);
    this.checkPrefCheckbox("cookiesAndStorage", true);
    this.acceptDialog();
  };
  dh.onunload = async () => {
    await promiseSanitized;
  };
  dh.open();
  await dh.promiseClosed;

  // Data for example.org should be cleared
  ok(
    !(await SiteDataTestUtils.hasIndexedDB("https://example.org")),
    "We don't have indexedDB data for example.org"
  );
  // Data for example.com should be cleared
  ok(
    !(await SiteDataTestUtils.hasIndexedDB("https://example.com")),
    "We don't have indexedDB data for example.com"
  );

  // Restore the sandbox after the test is complete
  sandbox.restore();
});

// tests the dialog resizing upon wrapping of text
// so that the clear buttons do not get cut out of the dialog.
add_task(async function testPossibleWrappingOfDialog() {
  await blankSlate();

  let dh = new ClearHistoryDialogHelper({
    checkingDataSizes: true,
  });
  // Create a sandbox for isolated stubbing within the test
  let sandbox = sinon.createSandbox();
  sandbox
    .stub(SiteDataManager, "getQuotaUsageForTimeRanges")
    .callsFake(async () => {
      info("stubbed getQuotaUsageForTimeRanges called");

      return {
        TIMESPAN_HOUR: 0,
        TIMESPAN_2HOURS: 0,
        TIMESPAN_4HOURS: LARGE_USAGE_NUM,
        TIMESPAN_TODAY: 0,
        TIMESPAN_EVERYTHING: 0,
      };
    });

  dh.onload = async function () {
    let windowObj =
      window.browsingContext.topChromeWindow.gDialogBox._dialog._frame
        .contentWindow;
    let promise = new Promise(resolve => {
      windowObj.addEventListener("resize", resolve);
    });
    this.selectDuration(Sanitizer.TIMESPAN_4HOURS);

    await promise;
    ok(
      !isIframeOverflowing(windowObj.document.getElementById("SanitizeDialog")),
      "There should be no overflow on wrapping in the dialog"
    );

    this.selectDuration(Sanitizer.TIMESPAN_2HOURS);
    await promise;
    ok(
      !isIframeOverflowing(windowObj.document.getElementById("SanitizeDialog")),
      "There should be no overflow on wrapping in the dialog"
    );

    this.cancelDialog();
  };
  dh.open();
  await dh.promiseClosed;

  // Restore the sandbox after the test is complete
  sandbox.restore();
});
