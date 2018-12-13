/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Main tests for SearchTelemetry - general engine visiting and link clicking.
 */

"use strict";

const {SearchTelemetry} = ChromeUtils.import("resource:///modules/SearchTelemetry.jsm", null);

const TEST_PROVIDER_INFO = {
  "example": {
    "regexp": /^http:\/\/mochi.test:.+\/browser\/browser\/components\/search\/test\/browser\/searchTelemetry(?:Ad)?.html/,
    "queryParam": "s",
    "codeParam": "abc",
    "codePrefixes": ["ff"],
    "followonParams": ["a"],
    "extraAdServersRegexps": [/^https:\/\/example\.com\/ad2?/],
  },
};

const SEARCH_AD_CLICK_SCALARS = [
  "browser.search.with_ads",
  "browser.search.ad_clicks",
];

function getPageUrl(useExample = false, useAdPage = false) {
  let server = useExample ? "example.com" : "mochi.test:8888";
  let page = useAdPage ? "searchTelemetryAd.html" : "searchTelemetry.html";
  return `http://${server}/browser/browser/components/search/test/browser/${page}`;
}

function getSERPUrl(page) {
  return page + "?s=test&abc=ff";
}

function getSERPFollowOnUrl(page) {
  return page + "?s=test&abc=ff&a=foo";
}

const searchCounts = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS");

async function assertTelemetry(expectedHistograms, expectedScalars) {
  let histSnapshot = {};
  let scalars = {};

  await TestUtils.waitForCondition(() => {
    histSnapshot = searchCounts.snapshot();
    return Object.getOwnPropertyNames(histSnapshot).length ==
      Object.getOwnPropertyNames(expectedHistograms).length;
  }, "should have the correct number of histograms");

  if (Object.entries(expectedScalars).length > 0) {
    await TestUtils.waitForCondition(() => {
      scalars = Services.telemetry.getSnapshotForKeyedScalars(
        "main", false).parent || {};
      return Object.getOwnPropertyNames(expectedScalars)[0] in
        scalars;
    }, "should have the expected keyed scalars");
  }

  Assert.equal(Object.getOwnPropertyNames(histSnapshot).length,
    Object.getOwnPropertyNames(expectedHistograms).length,
    "Should only have one key");

  for (let [key, value] of Object.entries(expectedHistograms)) {
    Assert.ok(key in histSnapshot,
      `Histogram should have the expected key: ${key}`);
    Assert.equal(histSnapshot[key].sum, value,
      `Should have counted the correct number of visits for ${key}`);
  }

  for (let [name, value] of Object.entries(expectedScalars)) {
    Assert.ok(name in scalars,
      `Scalar ${name} should have been added.`);
    Assert.deepEqual(scalars[name], value,
      `Should have counted the correct number of visits for ${name}`);
  }

  for (let name of SEARCH_AD_CLICK_SCALARS) {
    Assert.equal(name in scalars, name in expectedScalars,
      `Should have matched ${name} in scalars and expectedScalars`);
  }
}

// sharedData messages are only passed to the child on idle. Therefore
// we wait for a few idles to try and ensure the messages have been able
// to be passed across and handled.
async function waitForIdle() {
  for (let i = 0; i < 10; i++) {
    await new Promise(resolve => Services.tm.idleDispatchToMainThread(resolve));
  }
}

add_task(async function setup() {
  SearchTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO);
  await waitForIdle();
  // Enable local telemetry recording for the duration of the tests.
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;
  Services.prefs.setBoolPref("browser.search.log", true);

  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("browser.search.log");
    SearchTelemetry.overrideSearchTelemetryForTests();
    Services.telemetry.canRecordExtended = oldCanRecord;
    Services.telemetry.clearScalars();
  });
});

add_task(async function test_simple_search_page_visit() {
  searchCounts.clear();

  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: getSERPUrl(getPageUrl()),
  }, async () => {
    await assertTelemetry({"example.in-content:sap:ff": 1}, {});
  });
});

add_task(async function test_follow_on_visit() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: getSERPFollowOnUrl(getPageUrl()),
  }, async () => {
    await assertTelemetry({
      "example.in-content:sap:ff": 1,
      "example.in-content:sap-follow-on:ff": 1,
    }, {});
  });
});

add_task(async function test_track_ad() {
  searchCounts.clear();

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser,
    getSERPUrl(getPageUrl(false, true)));

  await assertTelemetry({"example.in-content:sap:ff": 1}, {
    "browser.search.with_ads": {"example": 1},
  });

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_track_ad_new_window() {
  searchCounts.clear();
  Services.telemetry.clearScalars();

  let win = await BrowserTestUtils.openNewBrowserWindow();

  let url = getSERPUrl(getPageUrl(false, true));
  await BrowserTestUtils.loadURI(win.gBrowser.selectedBrowser, url);
  await BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser, false, url);

  await assertTelemetry({"example.in-content:sap:ff": 1}, {
    "browser.search.with_ads": {"example": 1},
  });

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_track_ad_pages_without_ads() {
  // Note: the above tests have already checked a page with no ad-urls.
  searchCounts.clear();
  Services.telemetry.clearScalars();

  let tabs = [];

  tabs.push(await BrowserTestUtils.openNewForegroundTab(gBrowser,
    getSERPUrl(getPageUrl(false, false))));
  tabs.push(await BrowserTestUtils.openNewForegroundTab(gBrowser,
    getSERPUrl(getPageUrl(false, true))));

  await assertTelemetry({"example.in-content:sap:ff": 2}, {
    "browser.search.with_ads": {"example": 1},
  });

  for (let tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }
});

add_task(async function test_track_ad_click() {
  // Note: the above tests have already checked a page with no ad-urls.
  searchCounts.clear();
  Services.telemetry.clearScalars();

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser,
    getSERPUrl(getPageUrl(false, true)));

  await assertTelemetry({"example.in-content:sap:ff": 1}, {
    "browser.search.with_ads": {"example": 1},
  });

  let pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  await ContentTask.spawn(tab.linkedBrowser, {}, () => {
    content.document.getElementById("ad1").click();
  });
  await pageLoadPromise;

  await assertTelemetry({"example.in-content:sap:ff": 1}, {
    "browser.search.with_ads": {"example": 1},
    "browser.search.ad_clicks": {"example": 1},
  });

  // Now go back, and click again.
  pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  gBrowser.goBack();
  await pageLoadPromise;

  // We've gone back, so we register an extra display & if it is with ads or not.
  await assertTelemetry({"example.in-content:sap:ff": 2}, {
    "browser.search.with_ads": {"example": 2},
    "browser.search.ad_clicks": {"example": 1},
  });

  pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  await ContentTask.spawn(tab.linkedBrowser, {}, () => {
    content.document.getElementById("ad1").click();
  });
  await pageLoadPromise;

  await assertTelemetry({"example.in-content:sap:ff": 2}, {
    "browser.search.with_ads": {"example": 2},
    "browser.search.ad_clicks": {"example": 2},
  });

  BrowserTestUtils.removeTab(tab);
});
