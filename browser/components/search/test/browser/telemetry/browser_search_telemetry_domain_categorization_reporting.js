/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/*
 * This test ensures we are correctly reporting categorized domains from a SERP.
 */

ChromeUtils.defineESModuleGetters(this, {
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
  SearchSERPCategorization: "resource:///modules/SearchSERPTelemetry.sys.mjs",
  SearchSERPDomainToCategoriesMap:
    "resource:///modules/SearchSERPTelemetry.sys.mjs",
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
  sinon: "resource://testing-common/Sinon.sys.mjs",
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

const client = RemoteSettings(TELEMETRY_CATEGORIZATION_KEY);
const db = client.db;

let stub;
add_setup(async function () {
  SearchSERPTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO);
  await waitForIdle();

  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.log", true]],
  });

  stub = sinon.stub(SearchSERPCategorization, "dummyLogger");

  registerCleanupFunction(async () => {
    stub.restore();
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
  info("Load a sample SERP with organic results.");
  promise = waitForPageWithCategorizedDomains();
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await promise;

  // TODO: This needs to be refactored to actually test the reporting of the
  // categorization. (Bug 1854692)
  Assert.deepEqual(
    Array.from(stub.getCall(0).args[0]),
    ["foobar.org"],
    "Categorization of non-ads should match."
  );

  Assert.deepEqual(
    Array.from(stub.getCall(1).args[0]),
    ["abc.org", "def.org"],
    "Categorization of ads should match."
  );

  BrowserTestUtils.removeTab(tab);
  await client.attachments.cacheImpl.delete(record.id);
});

add_task(async function test_no_reporting_if_download_failure() {
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

  Assert.equal(
    stub.getCall(2),
    null,
    "dummyLogger should not have been called if attachments weren't downloaded."
  );

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_no_reporting_if_no_records() {
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

  Assert.equal(
    stub.getCall(2),
    null,
    "dummyLogger should not have been called if there are no records."
  );

  stub.restore();
  BrowserTestUtils.removeTab(tab);
});

// Per a request from Data Science, we need to limit the number of domains
// categorized to 10 non ad domains and 10 ad domains.
add_task(async function test_reporting_limited_to_10_domains_of_each_kind() {
  resetTelemetry();
  stub = sinon.stub(SearchSERPCategorization, "applyCategorizationLogic");

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
  info("Load a sample SERP with organic results.");
  let domainsCategorizedPromise = waitForPageWithCategorizedDomains();
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  await domainsCategorizedPromise;

  Assert.deepEqual(
    Array.from(stub.getCall(0).args[0]),
    [
      "test1.com",
      "test2.com",
      "test3.com",
      "test4.com",
      "test5.com",
      "test6.com",
      "test7.com",
      "test8.com",
      "test9.com",
      "test10.com",
    ],
    "First call to applyCategorizationLogic should pass in 10 non-ad domains."
  );

  Assert.deepEqual(
    Array.from(stub.getCall(1).args[0]),
    [
      "foo.com",
      "bar.com",
      "baz.com",
      "qux.com",
      "abc.com",
      "def.com",
      "ghi.org",
      "jkl.org",
      "mno.org",
      "pqr.org",
    ],
    "Second call to applyCategorizationLogic should pass in 10 ad domains."
  );

  BrowserTestUtils.removeTab(tab);
});
