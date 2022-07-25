/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Main tests for SearchSERPTelemetry - general engine visiting and link clicking.
 */

"use strict";

const { BrowserSearchTelemetry } = ChromeUtils.importESModule(
  "resource:///modules/BrowserSearchTelemetry.sys.mjs"
);
const { SearchSERPTelemetry } = ChromeUtils.importESModule(
  "resource:///modules/SearchSERPTelemetry.sys.mjs"
);
const { UrlbarTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/UrlbarTestUtils.sys.mjs"
);
const { SearchTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SearchTestUtils.sys.mjs"
);

const TEST_PROVIDER_INFO = [
  {
    telemetryId: "example",
    searchPageRegexp: /^https:\/\/example.com\/browser\/browser\/components\/search\/test\/browser\/searchTelemetry(?:Ad)?.html/,
    queryParamName: "s",
    codeParamName: "abc",
    taggedCodes: ["ff"],
    followOnParamNames: ["a"],
    extraAdServersRegexps: [/^https:\/\/example\.com\/ad2?/],
  },
];

function getPageUrl(useAdPage = false) {
  let page = useAdPage ? "searchTelemetryAd.html" : "searchTelemetry.html";
  return `https://example.com/browser/browser/components/search/test/browser/${page}`;
}

/**
 * Returns the index of the first search suggestion in the urlbar results.
 *
 * @returns {number} An index, or -1 if there are no search suggestions.
 */
async function getFirstSuggestionIndex() {
  const matchCount = UrlbarTestUtils.getResultCount(window);
  for (let i = 0; i < matchCount; i++) {
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    if (
      result.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
      result.searchParams.suggestion
    ) {
      return i;
    }
  }
  return -1;
}

// sharedData messages are only passed to the child on idle. Therefore
// we wait for a few idles to try and ensure the messages have been able
// to be passed across and handled.
async function waitForIdle() {
  for (let i = 0; i < 10; i++) {
    await new Promise(resolve => Services.tm.idleDispatchToMainThread(resolve));
  }
}

SearchTestUtils.init(this);
UrlbarTestUtils.init(this);

let tab;

add_setup(async function() {
  searchCounts.clear();
  Services.telemetry.clearScalars();

  SearchSERPTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO);
  await waitForIdle();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.searches", true]],
  });
  // Enable local telemetry recording for the duration of the tests.
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;
  Services.prefs.setBoolPref("browser.search.log", true);

  let currentEngineName = (await Services.search.getDefault()).name;

  await SearchTestUtils.installSearchExtension({
    search_url: getPageUrl(true),
    search_url_get_params: "s={searchTerms}&abc=ff",
    suggest_url:
      "https://example.com/browser/browser/components/search/test/browser/searchSuggestionEngine.sjs",
    suggest_url_get_params: "query={searchTerms}",
  });
  let engine1 = Services.search.getEngineByName("Example");

  await Services.search.setDefault(engine1);

  tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  registerCleanupFunction(async () => {
    BrowserTestUtils.removeTab(tab);
    Services.prefs.clearUserPref("browser.search.log");
    SearchSERPTelemetry.overrideSearchTelemetryForTests();
    Services.telemetry.canRecordExtended = oldCanRecord;
    Services.telemetry.clearScalars();
    await Services.search.setDefault(
      Services.search.getEngineByName(currentEngineName)
    );
  });
});

// These tests are consecutive and intentionally build on the results of the
// previous test.

async function loadSearchPage() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "searchSuggestion",
  });
  let idx = await getFirstSuggestionIndex();
  Assert.greaterOrEqual(idx, 0, "there should be a first suggestion");
  let loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  while (idx--) {
    EventUtils.sendKey("down");
  }
  EventUtils.sendKey("return");
  await loadPromise;
}

add_task(async function test_search() {
  // Load a page via the address bar.
  await loadSearchPage();

  await assertSearchSourcesTelemetry(
    {
      "example.in-content:sap:ff": 1,
      "other-Example.urlbar": 1,
    },
    {
      "browser.search.content.urlbar": { "example:tagged:ff": 1 },
      "browser.search.with_ads": { "example:sap": 1 },
      "browser.search.withads.urlbar": { "example:tagged": 1 },
    }
  );
});

add_task(async function test_reload() {
  let promise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  tab.linkedBrowser.reload();
  await promise;
  await promiseWaitForAdLinkCheck();

  await assertSearchSourcesTelemetry(
    {
      "example.in-content:sap:ff": 2,
      "other-Example.urlbar": 1,
    },
    {
      "browser.search.content.urlbar": { "example:tagged:ff": 1 },
      "browser.search.content.reload": { "example:tagged:ff": 1 },
      "browser.search.with_ads": { "example:sap": 2 },
      "browser.search.withads.urlbar": { "example:tagged": 1 },
      "browser.search.withads.reload": { "example:tagged": 1 },
    }
  );

  let pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    content.document.getElementById("ad1").click();
  });
  await pageLoadPromise;

  await assertSearchSourcesTelemetry(
    {
      "example.in-content:sap:ff": 2,
      "other-Example.urlbar": 1,
    },
    {
      "browser.search.content.urlbar": { "example:tagged:ff": 1 },
      "browser.search.content.reload": { "example:tagged:ff": 1 },
      "browser.search.with_ads": { "example:sap": 2 },
      "browser.search.withads.urlbar": { "example:tagged": 1 },
      "browser.search.withads.reload": { "example:tagged": 1 },
      "browser.search.ad_clicks": { "example:sap": 1 },
      "browser.search.adclicks.reload": { "example:tagged": 1 },
    }
  );
});

let searchUrl;

add_task(async function test_fresh_search() {
  searchCounts.clear();
  Services.telemetry.clearScalars();

  // Load a page via the address bar.
  await loadSearchPage();

  searchUrl = tab.linkedBrowser.url;

  await assertSearchSourcesTelemetry(
    {
      "example.in-content:sap:ff": 1,
      "other-Example.urlbar": 1,
    },
    {
      "browser.search.content.urlbar": { "example:tagged:ff": 1 },
      "browser.search.with_ads": { "example:sap": 1 },
      "browser.search.withads.urlbar": { "example:tagged": 1 },
    }
  );
});

add_task(async function test_click_ad() {
  let pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    content.document.getElementById("ad1").click();
  });
  await pageLoadPromise;

  await assertSearchSourcesTelemetry(
    {
      "example.in-content:sap:ff": 1,
      "other-Example.urlbar": 1,
    },
    {
      "browser.search.content.urlbar": { "example:tagged:ff": 1 },
      "browser.search.with_ads": { "example:sap": 1 },
      "browser.search.withads.urlbar": { "example:tagged": 1 },
      "browser.search.ad_clicks": { "example:sap": 1 },
      "browser.search.adclicks.urlbar": { "example:tagged": 1 },
    }
  );
});

add_task(async function test_go_back() {
  let promise = BrowserTestUtils.waitForLocationChange(gBrowser, searchUrl);
  tab.linkedBrowser.goBack();
  await promise;
  await promiseWaitForAdLinkCheck();

  await assertSearchSourcesTelemetry(
    {
      "example.in-content:sap:ff": 2,
      "other-Example.urlbar": 1,
    },
    {
      "browser.search.content.urlbar": { "example:tagged:ff": 1 },
      "browser.search.content.tabhistory": { "example:tagged:ff": 1 },
      "browser.search.with_ads": { "example:sap": 2 },
      "browser.search.withads.urlbar": { "example:tagged": 1 },
      "browser.search.withads.tabhistory": { "example:tagged": 1 },
      "browser.search.ad_clicks": { "example:sap": 1 },
      "browser.search.adclicks.urlbar": { "example:tagged": 1 },
    }
  );

  let pageLoadPromise = BrowserTestUtils.waitForLocationChange(gBrowser);
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    content.document.getElementById("ad1").click();
  });
  await pageLoadPromise;

  await assertSearchSourcesTelemetry(
    {
      "example.in-content:sap:ff": 2,
      "other-Example.urlbar": 1,
    },
    {
      "browser.search.content.urlbar": { "example:tagged:ff": 1 },
      "browser.search.content.tabhistory": { "example:tagged:ff": 1 },
      "browser.search.with_ads": { "example:sap": 2 },
      "browser.search.withads.urlbar": { "example:tagged": 1 },
      "browser.search.withads.tabhistory": { "example:tagged": 1 },
      "browser.search.ad_clicks": { "example:sap": 2 },
      "browser.search.adclicks.urlbar": { "example:tagged": 1 },
      "browser.search.adclicks.tabhistory": { "example:tagged": 1 },
    }
  );
});
