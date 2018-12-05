/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Main tests for SearchTelemetry - general engine visiting and link clicking.
 */

"use strict";

const {SearchTelemetry} = ChromeUtils.import("resource:///modules/SearchTelemetry.jsm", null);

const TEST_PROVIDER_INFO = {
  "example": {
    "regexp": /^http:\/\/mochi.test:.+\/browser\/browser\/components\/search\/test\/browser\/searchTelemetry.html/,
    "queryParam": "s",
    "codeParam": "abc",
    "codePrefixes": ["ff"],
    "followonParams": ["a"],
    "extraAdServersRegexp": /^https:\/\/example\.com/,
    "adPrefixes": ["ad", "ad2"],
  },
};

const MAIN_TEST_PAGE =
  "http://mochi.test:8888/browser/browser/components/search/test/browser/searchTelemetry.html";
const TEST_PROVIDER_SERP_URL =
  MAIN_TEST_PAGE + "?s=test&abc=ff";
const TEST_PROVIDER_SERP_FOLLOWON_URL =
  MAIN_TEST_PAGE + "?s=test&abc=ff&a=foo";

const searchCounts = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS");

async function assertTelemetry(expectedHistograms) {
  let histSnapshot;

  await TestUtils.waitForCondition(() => {
    histSnapshot = searchCounts.snapshot();
    return Object.getOwnPropertyNames(histSnapshot).length ==
      Object.getOwnPropertyNames(expectedHistograms).length;
  });

  Assert.equal(Object.getOwnPropertyNames(histSnapshot).length,
    Object.getOwnPropertyNames(expectedHistograms).length,
    "Should only have one key");

  for (let [key, value] of Object.entries(expectedHistograms)) {
    Assert.ok(key in histSnapshot,
      `Histogram should have the expected key: ${key}`);
    Assert.equal(histSnapshot[key].sum, value,
      `Should have counted the correct number of visits for ${key}`);
  }
}

add_task(async function setup() {
  SearchTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO);
  // Enable local telemetry recording for the duration of the tests.
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;
  Services.prefs.setBoolPref("browser.search.log", true);

  registerCleanupFunction(async () => {
    SearchTelemetry.overrideSearchTelemetryForTests();
    Services.telemetry.canRecordExtended = oldCanRecord;
    Services.telemetry.clearScalars();
  });
});

add_task(async function test_simple_search_page_visit() {
  searchCounts.clear();

  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: TEST_PROVIDER_SERP_URL,
  }, async () => {
    await assertTelemetry({"example.in-content:sap:ff": 1});
  });
});

add_task(async function test_follow_on_visit() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: TEST_PROVIDER_SERP_FOLLOWON_URL,
  }, async () => {
    await assertTelemetry({
      "example.in-content:sap:ff": 1,
      "example.in-content:sap-follow-on:ff": 1,
    });
  });
});
