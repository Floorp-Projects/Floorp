/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/*
 * This test ensures we are correctly restarting a download of an attachment
 * after a failure. We simulate failures by not caching the attachment in
 * Remote Settings.
 */

ChromeUtils.defineESModuleGetters(this, {
  TELEMETRY_CATEGORIZATION_DOWNLOAD_SETTINGS:
    "resource:///modules/SearchSERPTelemetry.sys.mjs",
  SearchSERPDomainToCategoriesMap:
    "resource:///modules/SearchSERPTelemetry.sys.mjs",
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
});

const TEST_PROVIDER_INFO = [
  {
    telemetryId: "example",
    searchPageRegexp:
      /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/telemetry\/searchTelemetry/,
    queryParamNames: ["s"],
    codeParamName: "abc",
    taggedCodes: ["ff"],
    adServerAttributes: ["mozAttr"],
    nonAdsLinkRegexps: [/^https:\/\/example.com/],
    extraAdServersRegexps: [/^https:\/\/example\.com\/ad/],
    // The search telemetry entry responsible for targeting the specific results.
    domainExtraction: {
      ads: [
        {
          selectors: "[data-ad-domain]",
          method: "data-attribute",
          options: {
            dataAttributeKey: "adDomain",
          },
        },
        {
          selectors: ".ad",
          method: "href",
          options: {
            queryParamKey: "ad_domain",
          },
        },
      ],
      nonAds: [
        {
          selectors: "#results .organic a",
          method: "href",
        },
      ],
    },
    components: [
      {
        type: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
        default: true,
      },
    ],
  },
];

function waitForDownloadError() {
  return TestUtils.consoleMessageObserved(msg => {
    return (
      typeof msg.wrappedJSObject.arguments?.[0] == "string" &&
      msg.wrappedJSObject.arguments[0].includes("Could not download file:")
    );
  });
}

const client = RemoteSettings(TELEMETRY_CATEGORIZATION_KEY);
const db = client.db;

// Shorten the timer so that tests don't have to wait too long.
const TIMEOUT_IN_MS = 250;
add_setup(async function () {
  SearchSERPTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO);
  await waitForIdle();

  await db.clear();

  // Set the state of the pref to false so that tests toggle the preference,
  // triggering the map to be updated.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.serpEventTelemetryCategorization.enabled", false]],
  });

  let defaultDownloadSettings = {
    ...TELEMETRY_CATEGORIZATION_DOWNLOAD_SETTINGS,
  };

  // Use a much shorter interval from the default preference that when we
  // simulate download failures, we don't have to wait long before another
  // download attempt.
  TELEMETRY_CATEGORIZATION_DOWNLOAD_SETTINGS.base = TIMEOUT_IN_MS;

  // Normally we add random time to avoid a failure resulting in everyone
  // hitting the network at once. For tests, we remove this unless explicitly
  // testing.
  TELEMETRY_CATEGORIZATION_DOWNLOAD_SETTINGS.minAdjust = 0;
  TELEMETRY_CATEGORIZATION_DOWNLOAD_SETTINGS.maxAdjust = 0;

  registerCleanupFunction(async () => {
    SearchSERPTelemetry.overrideSearchTelemetryForTests();
    resetTelemetry();
    TELEMETRY_CATEGORIZATION_DOWNLOAD_SETTINGS = {
      ...defaultDownloadSettings,
    };
  });
});

add_task(async function test_download_after_failure() {
  // Most cases, we should use a convenience function, but in this case,
  // we want to explictly "forget" to include an attachment to cause a failure.
  let { record, attachment } = await mockRecordWithAttachment({
    id: "example_id",
    version: 1,
    filename: "domain_category_mappings.json",
  });
  await db.create(record);
  await db.importChanges({}, Date.now());

  let downloadError = waitForDownloadError();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.serpEventTelemetryCategorization.enabled", true]],
  });
  await downloadError;

  // In between the download failure and one of download retries, cache
  // the attachment so that the next download attempt will be successful.
  client.attachments.cacheImpl.set(record.id, attachment);
  await TestUtils.topicObserved("domain-to-categories-map-update-complete");

  let url = getSERPUrl("searchTelemetryDomainCategorizationReporting.html");
  info("Load a sample SERP with organic and sponsored results.");
  let promise = waitForPageWithCategorizedDomains();
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await promise;

  await BrowserTestUtils.removeTab(tab);
  assertCategorizationValues([
    {
      organic_category: "3",
      organic_num_domains: "1",
      organic_num_inconclusive: "0",
      organic_num_unknown: "0",
      sponsored_category: "4",
      sponsored_num_domains: "2",
      sponsored_num_inconclusive: "0",
      sponsored_num_unknown: "0",
      mappings_version: "1",
      app_version: APP_MAJOR_VERSION,
      channel: CHANNEL,
      region: REGION,
      partner_code: "ff",
      provider: "example",
      tagged: "true",
      num_ads_visible: "2",
      num_ads_clicked: "0",
    },
  ]);

  // Clean up.
  await SpecialPowers.popPrefEnv();
  await resetCategorizationCollection(record);
});

add_task(async function test_download_after_multiple_failures() {
  let { record } = await mockRecordWithAttachment({
    id: "example_id",
    version: 1,
    filename: "domain_category_mappings.json",
  });
  await db.create(record);
  await db.importChanges({}, Date.now());

  let downloadError = waitForDownloadError();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.serpEventTelemetryCategorization.enabled", true]],
  });
  await downloadError;

  // Following an initial download failure, the number of allowable retries
  // should equal to the maximum number per session.
  for (
    let i = 0;
    i < TELEMETRY_CATEGORIZATION_DOWNLOAD_SETTINGS.maxTriesPerSession;
    ++i
  ) {
    await waitForDownloadError();
  }

  // To ensure we didn't attempt another download, wait more than what another
  // download error should take.
  let consoleObserved = false;
  let timeout = false;
  let firstPromise = waitForDownloadError().then(() => {
    consoleObserved = true;
  });
  let secondPromise = new Promise(resolve =>
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    setTimeout(resolve, TIMEOUT_IN_MS + 100)
  ).then(() => (timeout = true));
  await Promise.race([firstPromise, secondPromise]);
  Assert.equal(consoleObserved, false, "Encountered download failure");
  Assert.equal(timeout, true, "Timeout occured");

  Assert.ok(SearchSERPDomainToCategoriesMap.empty, "Map is empty");

  // Clean up.
  await SpecialPowers.popPrefEnv();
  await resetCategorizationCollection(record);
});

add_task(async function test_cancel_download_timer() {
  let { record } = await mockRecordWithAttachment({
    id: "example_id",
    version: 1,
    filename: "domain_category_mappings.json",
  });
  await db.create(record);
  await db.importChanges({}, Date.now());

  let downloadError = waitForDownloadError();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.serpEventTelemetryCategorization.enabled", true]],
  });
  await downloadError;

  // Changing the gating preference to false before the map is populated
  // should cancel the download timer.
  let observeCancel = TestUtils.consoleMessageObserved(msg => {
    return (
      typeof msg.wrappedJSObject.arguments?.[0] == "string" &&
      msg.wrappedJSObject.arguments[0].includes(
        "Cancel and nullify download timer."
      )
    );
  });
  await SpecialPowers.popPrefEnv();
  await observeCancel;

  // To ensure we don't attempt another download, wait a bit over how long the
  // the download error should take.
  let consoleObserved = false;
  let timeout = false;
  let firstPromise = waitForDownloadError().then(() => {
    consoleObserved = true;
  });
  let secondPromise = new Promise(resolve =>
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    setTimeout(resolve, TIMEOUT_IN_MS + 100)
  ).then(() => (timeout = true));
  await Promise.race([firstPromise, secondPromise]);
  Assert.equal(consoleObserved, false, "Encountered download failure");
  Assert.equal(timeout, true, "Timeout occured");
  Assert.ok(SearchSERPDomainToCategoriesMap.empty, "Map is empty");

  // Clean up.
  await SpecialPowers.popPrefEnv();
  await resetCategorizationCollection(record);
});

add_task(async function test_download_adjust() {
  // To test that we're actually adding a random delay to the base value,
  // we set the base number to zero so that the next attempt should be
  // instant but we'll wait in between 0 and 1000ms and expect the
  // timer to elapse first.
  TELEMETRY_CATEGORIZATION_DOWNLOAD_SETTINGS.base = 0;
  TELEMETRY_CATEGORIZATION_DOWNLOAD_SETTINGS.minAdjust = 1000;
  TELEMETRY_CATEGORIZATION_DOWNLOAD_SETTINGS.maxAdjust = 1000;

  let { record } = await mockRecordWithAttachment({
    id: "example_id",
    version: 1,
    filename: "domain_category_mappings.json",
  });
  await db.create(record);
  await db.importChanges({}, Date.now());

  let downloadError = waitForDownloadError();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.serpEventTelemetryCategorization.enabled", true]],
  });
  await downloadError;

  // The timer should finish before the next error.
  let consoleObserved = false;
  let timeout = false;
  let firstPromise = waitForDownloadError().then(() => {
    consoleObserved = true;
  });
  let secondPromise = new Promise(resolve =>
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    setTimeout(resolve, 250)
  ).then(() => (timeout = true));
  await Promise.race([firstPromise, secondPromise]);
  Assert.equal(timeout, true, "Timeout occured");
  Assert.equal(consoleObserved, false, "Encountered download failure");

  await firstPromise;
  Assert.equal(consoleObserved, true, "Encountered download failure");

  // Clean up.
  await SpecialPowers.popPrefEnv();
  await resetCategorizationCollection(record);
  TELEMETRY_CATEGORIZATION_DOWNLOAD_SETTINGS.base = TIMEOUT_IN_MS;
  TELEMETRY_CATEGORIZATION_DOWNLOAD_SETTINGS.minAdjust = 0;
  TELEMETRY_CATEGORIZATION_DOWNLOAD_SETTINGS.maxAdjust = 0;
});
