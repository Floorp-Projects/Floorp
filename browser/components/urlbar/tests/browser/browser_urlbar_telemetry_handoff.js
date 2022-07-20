/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { SearchSERPTelemetry } = ChromeUtils.importESModule(
  "resource:///modules/SearchSERPTelemetry.sys.mjs"
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

// sharedData messages are only passed to the child on idle. Therefore
// we wait for a few idles to try and ensure the messages have been able
// to be passed across and handled.
async function waitForIdle() {
  for (let i = 0; i < 10; i++) {
    await new Promise(resolve => Services.tm.idleDispatchToMainThread(resolve));
  }
}

add_setup(async function() {
  SearchSERPTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO);
  await waitForIdle();

  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.newtabpage.activity-stream.improvesearch.handoffToAwesomebar",
        true,
      ],
    ],
  });

  await SearchTestUtils.installSearchExtension({
    search_url: getPageUrl(true),
    search_url_get_params: "s={searchTerms}&abc=ff",
    suggest_url:
      "https://example.com/browser/browser/components/search/test/browser/searchSuggestionEngine.sjs",
    suggest_url_get_params: "query={searchTerms}",
  });
  const testEngine = Services.search.getEngineByName("Example");
  const originalEngine = await Services.search.getDefault();
  await Services.search.setDefault(testEngine);

  const oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;
  Services.telemetry.setEventRecordingEnabled("navigation", true);

  registerCleanupFunction(async function() {
    await Services.search.setDefault(originalEngine);
    await PlacesUtils.history.clear();

    Services.telemetry.canRecordExtended = oldCanRecord;
    Services.telemetry.setEventRecordingEnabled("navigation", false);

    SearchSERPTelemetry.overrideSearchTelemetryForTests();

    Services.telemetry.clearScalars();
    Services.telemetry.clearEvents();
  });
});

add_task(async function test_search() {
  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();

  const histogram = TelemetryTestUtils.getAndClearKeyedHistogram(
    "SEARCH_COUNTS"
  );

  info("Load about:newtab in new window");
  const newtab = "about:newtab";
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  BrowserTestUtils.loadURI(tab.linkedBrowser, newtab);
  await BrowserTestUtils.browserStopped(tab.linkedBrowser, newtab);

  info("Focus on search input in newtab content");
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    const searchInput = content.document.querySelector(".fake-editable");
    searchInput.click();
  });

  info("Search and wait the result");
  const onLoaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  EventUtils.synthesizeKey("q");
  EventUtils.synthesizeKey("VK_RETURN");
  await onLoaded;

  info("Check the telemetries");
  await assertScalars([
    ["browser.engagement.navigation.urlbar_handoff", "search_enter", 1],
  ]);
  await assertHistogram(histogram, [
    ["example.in-content:sap:ff", 1],
    ["other-Example.urlbar-handoff", 1],
  ]);
  TelemetryTestUtils.assertEvents(
    [
      [
        "navigation",
        "search",
        "urlbar_handoff",
        "enter",
        { engine: "other-Example" },
      ],
    ],
    { category: "navigation", method: "search" }
  );

  BrowserTestUtils.removeTab(tab);
});

async function assertHistogram(histogram, expectedResults) {
  await TestUtils.waitForCondition(() => {
    const snapshot = histogram.snapshot();
    return expectedResults.every(([key]) => key in snapshot);
  }, "Wait until the histogram has expected keys");

  for (const [key, value] of expectedResults) {
    TelemetryTestUtils.assertKeyedHistogramSum(histogram, key, value);
  }
}

async function assertScalars(expectedResults) {
  await TestUtils.waitForCondition(() => {
    const scalars = TelemetryTestUtils.getProcessScalars("parent", true);
    return expectedResults.every(([scalarName]) => scalarName in scalars);
  }, "Wait until the scalars have expected keyed scalars");

  const scalars = TelemetryTestUtils.getProcessScalars("parent", true);

  for (const [scalarName, key, value] of expectedResults) {
    TelemetryTestUtils.assertKeyedScalar(scalars, scalarName, key, value);
  }
}
