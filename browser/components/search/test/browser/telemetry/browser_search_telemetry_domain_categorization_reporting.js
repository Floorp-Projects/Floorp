/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/*
 * This test ensures we are correctly reporting categorized domains from a SERP.
 */

ChromeUtils.defineESModuleGetters(this, {
  CATEGORIZATION_SETTINGS: "resource:///modules/SearchSERPTelemetry.sys.mjs",
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
  TELEMETRY_CATEGORIZATION_KEY:
    "resource:///modules/SearchSERPTelemetry.sys.mjs",
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
    nonAdsLinkRegexps: [],
    extraAdServersRegexps: [
      /^https:\/\/example\.com\/ad/,
      /^https:\/\/www\.test(1[3456789]|2[01234])\.com/,
    ],
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

const client = RemoteSettings(TELEMETRY_CATEGORIZATION_KEY);
const db = client.db;

add_setup(async function () {
  SearchSERPTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO);
  await waitForIdle();

  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.log", true]],
  });

  registerCleanupFunction(async () => {
    SearchSERPTelemetry.overrideSearchTelemetryForTests();
    resetTelemetry();
    await db.clear();
  });
});

add_task(async function test_categorization_reporting() {
  resetTelemetry();

  await db.clear();
  let { record, attachment } = await mockRecordWithAttachment({
    id: "example_id",
    version: 1,
    filename: "domain_category_mappings.json",
  });
  await db.create(record);
  await client.attachments.cacheImpl.set(record.id, attachment);
  await db.importChanges({}, Date.now());

  let promise = TestUtils.topicObserved(
    "domain-to-categories-map-update-complete"
  );
  // Enabling the preference will trigger filling the map.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.serpEventTelemetryCategorization.enabled", true]],
  });
  await promise;

  let url = getSERPUrl("searchTelemetryDomainCategorizationReporting.html");
  info("Load a sample SERP with organic and sponsored results.");
  promise = waitForPageWithCategorizedDomains();
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
      num_ads_clicked: "0",
      num_ads_visible: "2",
    },
  ]);

  await client.attachments.cacheImpl.delete(record.id);
});

add_task(async function test_no_reporting_if_download_failure() {
  resetTelemetry();

  await db.clear();
  let { record } = await mockRecordWithAttachment({
    id: "example_id",
    version: 1,
    filename: "domain_category_mappings.json",
  });
  await db.create(record);
  await db.importChanges({}, Date.now());

  const payload = {
    current: [record],
    created: [],
    updated: [record],
    deleted: [],
  };

  let observeDownloadError = TestUtils.consoleMessageObserved(msg => {
    return (
      typeof msg.wrappedJSObject.arguments?.[0] == "string" &&
      msg.wrappedJSObject.arguments[0].includes("Could not download file:")
    );
  });
  // Since the preference is already enabled, and the map is filled we trigger
  // the map to be updated via an RS sync. The download failure should cause the
  // map to remain empty.
  await client.emit("sync", { data: payload });
  await observeDownloadError;

  let url = getSERPUrl("searchTelemetryDomainCategorizationReporting.html");
  info("Load a sample SERP with organic results.");
  let promise = waitForPageWithCategorizedDomains();
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await promise;

  await BrowserTestUtils.removeTab(tab);
  assertCategorizationValues([]);
});

add_task(async function test_no_reporting_if_no_records() {
  resetTelemetry();

  const payload = {
    current: [],
    created: [],
    updated: [],
    deleted: [],
  };
  let observeNoRecords = TestUtils.consoleMessageObserved(msg => {
    return (
      typeof msg.wrappedJSObject.arguments?.[0] == "string" &&
      msg.wrappedJSObject.arguments[0].includes(
        "No records found for domain-to-categories map."
      )
    );
  });
  await client.emit("sync", { data: payload });
  await observeNoRecords;

  let url = getSERPUrl("searchTelemetryDomainCategorizationReporting.html");
  info("Load a sample SERP with organic results.");
  let promise = waitForPageWithCategorizedDomains();
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await promise;

  await BrowserTestUtils.removeTab(tab);
  assertCategorizationValues([]);
});

// Per a request from Data Science, we need to limit the number of domains
// categorized to 10 non ad domains and 10 ad domains.
add_task(async function test_reporting_limited_to_10_domains_of_each_kind() {
  resetTelemetry();

  await db.clear();
  let { record, attachment } = await mockRecordWithAttachment({
    id: "example_id",
    version: 1,
    filename: "domain_category_mappings.json",
  });
  await db.create(record);
  await client.attachments.cacheImpl.set(record.id, attachment);
  await db.importChanges({}, Date.now());

  let mapUpdatedPromise = TestUtils.topicObserved(
    "domain-to-categories-map-update-complete"
  );
  const payload = {
    current: [record],
    created: [record],
    updated: [],
    deleted: [],
  };
  await client.emit("sync", { data: payload });
  await mapUpdatedPromise;

  let url = getSERPUrl(
    "searchTelemetryDomainCategorizationCapProcessedDomains.html"
  );
  info(
    "Load a sample SERP with more than 10 organic results and more than 10 sponsored results."
  );
  let domainsCategorizedPromise = waitForPageWithCategorizedDomains();
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await domainsCategorizedPromise;

  await BrowserTestUtils.removeTab(tab);

  assertCategorizationValues([
    {
      organic_category: "0",
      organic_num_domains:
        CATEGORIZATION_SETTINGS.MAX_DOMAINS_TO_CATEGORIZE.toString(),
      organic_num_inconclusive: "0",
      organic_num_unknown: "10",
      sponsored_category: "2",
      sponsored_num_domains:
        CATEGORIZATION_SETTINGS.MAX_DOMAINS_TO_CATEGORIZE.toString(),
      sponsored_num_inconclusive: "0",
      sponsored_num_unknown: "8",
      mappings_version: "1",
      num_ads_clicked: "0",
      num_ads_visible: "12",
    },
  ]);
});
