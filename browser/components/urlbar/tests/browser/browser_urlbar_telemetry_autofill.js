/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests urlbar autofill telemetry.
 */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarProviderPreloadedSites:
    "resource:///modules/UrlbarProviderPreloadedSites.jsm",
});

const SCALAR_URLBAR = "browser.engagement.navigation.urlbar";

function assertSearchTelemetryEmpty(search_hist) {
  const scalars = TelemetryTestUtils.getProcessScalars("parent", true, false);
  Assert.ok(
    !(SCALAR_URLBAR in scalars),
    `Should not have recorded ${SCALAR_URLBAR}`
  );

  // SEARCH_COUNTS should not contain any engine counts at all. The keys in this
  // histogram are search engine telemetry identifiers.
  Assert.deepEqual(
    Object.keys(search_hist.snapshot()),
    [],
    "SEARCH_COUNTS is empty"
  );

  // Also check events.
  let events = Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
    false
  );
  events = (events.parent || []).filter(
    e => e[1] == "navigation" && e[2] == "search"
  );
  Assert.deepEqual(
    events,
    [],
    "Should not have recorded any navigation search events"
  );
}

function snapshotHistograms() {
  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();
  return {
    resultIndexHist: TelemetryTestUtils.getAndClearHistogram(
      "FX_URLBAR_SELECTED_RESULT_INDEX"
    ),
    resultTypeHist: TelemetryTestUtils.getAndClearHistogram(
      "FX_URLBAR_SELECTED_RESULT_TYPE_2"
    ),
    resultIndexByTypeHist: TelemetryTestUtils.getAndClearKeyedHistogram(
      "FX_URLBAR_SELECTED_RESULT_INDEX_BY_TYPE_2"
    ),
    resultMethodHist: TelemetryTestUtils.getAndClearHistogram(
      "FX_URLBAR_SELECTED_RESULT_METHOD"
    ),
    search_hist: TelemetryTestUtils.getAndClearKeyedHistogram("SEARCH_COUNTS"),
  };
}

function assertTelemetryResults(histograms, type, index, method) {
  TelemetryTestUtils.assertHistogram(histograms.resultIndexHist, index, 1);

  TelemetryTestUtils.assertHistogram(
    histograms.resultTypeHist,
    UrlbarUtils.SELECTED_RESULT_TYPES[type],
    1
  );

  TelemetryTestUtils.assertKeyedHistogramValue(
    histograms.resultIndexByTypeHist,
    type,
    index,
    1
  );

  TelemetryTestUtils.assertHistogram(histograms.resultMethodHist, method, 1);

  TelemetryTestUtils.assertKeyedScalar(
    TelemetryTestUtils.getProcessScalars("parent", true, true),
    `urlbar.picked.${type}`,
    index,
    1
  );
}

/**
 * Performs a search and picks the first result. The search string is assumed
 * to trigger an autofill result.
 *
 * @param {string} searchString
 * @param {string} autofilledValue
 *   The input's expected value after autofill occurs.
 */
async function triggerAutofillAndPickResult(searchString, autofilledValue) {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: searchString,
      fireInputEvent: true,
    });
    let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
    Assert.ok(details.autofill, "Result is autofill");
    Assert.equal(gURLBar.value, autofilledValue, "gURLBar.value");
    Assert.equal(gURLBar.selectionStart, searchString.length, "selectionStart");
    Assert.equal(gURLBar.selectionEnd, autofilledValue.length, "selectionEnd");

    let loadPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    EventUtils.synthesizeKey("KEY_Enter");
    await loadPromise;

    let url = autofilledValue.includes(":")
      ? autofilledValue
      : "http://" + autofilledValue;
    Assert.equal(gBrowser.currentURI.spec, url, "Loaded URL is correct");
  });
}

// Allow more time for Mac machines so they don't time out in verify mode.
if (AppConstants.platform == "macosx") {
  requestLongerTimeout(3);
}

add_setup(async function() {
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();

  registerCleanupFunction(async () => {
    await PlacesUtils.history.clear();
  });
});

// Checks adaptive history, origin, and URL autofill.
add_task(async function history() {
  const testData = [
    {
      adaptiveHistoryPref: true,
      visitHistory: ["http://example.com/test"],
      inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
      userInput: "ex",
      autofilled: "example.com/",
      expected: "autofill_origin",
    },
    {
      adaptiveHistoryPref: true,
      visitHistory: ["http://example.com/test"],
      inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
      userInput: "exa",
      autofilled: "example.com/test",
      expected: "autofill_adaptive",
    },
    {
      adaptiveHistoryPref: true,
      visitHistory: ["http://example.com/test"],
      inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
      userInput: "exam",
      autofilled: "example.com/test",
      expected: "autofill_adaptive",
    },
    {
      adaptiveHistoryPref: true,
      visitHistory: ["http://example.com/test"],
      inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
      userInput: "example.com",
      autofilled: "example.com/test",
      expected: "autofill_adaptive",
    },
    {
      adaptiveHistoryPref: true,
      visitHistory: ["http://example.com/test"],
      inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
      userInput: "example.com/",
      autofilled: "example.com/test",
      expected: "autofill_adaptive",
    },
    {
      adaptiveHistoryPref: true,
      visitHistory: ["http://example.com/test"],
      inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
      userInput: "example.com/test",
      autofilled: "example.com/test",
      expected: "autofill_adaptive",
    },
    {
      adaptiveHistoryPref: true,
      visitHistory: ["http://example.com/test", "http://example.org/test"],
      inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
      userInput: "example.org",
      autofilled: "example.org/",
      expected: "autofill_origin",
    },
    {
      adaptiveHistoryPref: true,
      visitHistory: ["http://example.com/test", "http://example.com/test/url"],
      inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
      userInput: "example.com/test/",
      autofilled: "example.com/test/",
      expected: "autofill_url",
    },
    {
      adaptiveHistoryPref: true,
      visitHistory: [{ uri: "http://example.com/test" }],
      inputHistory: [
        { uri: "http://example.com/test", input: "http://example.com/test" },
      ],
      userInput: "http://example.com/test",
      autofilled: "http://example.com/test",
      expected: "autofill_adaptive",
    },
    {
      adaptiveHistoryPref: false,
      visitHistory: [{ uri: "http://example.com/test" }],
      inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
      userInput: "example",
      autofilled: "example.com/",
      expected: "autofill_origin",
    },
    {
      adaptiveHistoryPref: false,
      visitHistory: [{ uri: "http://example.com/test" }],
      inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
      userInput: "example.com/te",
      autofilled: "example.com/test",
      expected: "autofill_url",
    },
  ];

  for (const {
    adaptiveHistoryPref,
    visitHistory,
    inputHistory,
    userInput,
    autofilled,
    expected,
  } of testData) {
    const histograms = snapshotHistograms();

    await PlacesTestUtils.addVisits(visitHistory);
    for (const { uri, input } of inputHistory) {
      await UrlbarUtils.addToInputHistory(uri, input);
    }

    UrlbarPrefs.set("autoFill.adaptiveHistory.enabled", adaptiveHistoryPref);

    await triggerAutofillAndPickResult(userInput, autofilled);

    assertSearchTelemetryEmpty(histograms.search_hist);
    assertTelemetryResults(
      histograms,
      expected,
      0,
      UrlbarTestUtils.SELECTED_RESULT_METHODS.enter
    );

    UrlbarPrefs.clear("autoFill.adaptiveHistory.enabled");
    await PlacesUtils.history.clear();
  }
});

// Checks about-page autofill (e.g., "about:about").
add_task(async function about() {
  let histograms = snapshotHistograms();
  await triggerAutofillAndPickResult("about:abou", "about:about");

  assertSearchTelemetryEmpty(histograms.search_hist);
  assertTelemetryResults(
    histograms,
    "autofill_about",
    0,
    UrlbarTestUtils.SELECTED_RESULT_METHODS.enter
  );

  await PlacesUtils.history.clear();
});

// Checks preloaded sites autofill.
add_task(async function preloaded() {
  UrlbarPrefs.set("usepreloadedtopurls.enabled", true);
  UrlbarPrefs.set("usepreloadedtopurls.expire_days", 100);
  UrlbarProviderPreloadedSites.populatePreloadedSiteStorage([
    ["http://example.com/", "Example"],
  ]);

  let histograms = snapshotHistograms();
  await triggerAutofillAndPickResult("example", "example.com/");

  assertSearchTelemetryEmpty(histograms.search_hist);
  assertTelemetryResults(
    histograms,
    "autofill_preloaded",
    0,
    UrlbarTestUtils.SELECTED_RESULT_METHODS.enter
  );

  await PlacesUtils.history.clear();
  UrlbarPrefs.clear("usepreloadedtopurls.enabled");
  UrlbarPrefs.clear("usepreloadedtopurls.expire_days");
});

// Checks the "other" fallback, which shouldn't normally happen.
add_task(async function other() {
  let searchString = "exam";
  let autofilledValue = "example.com/";

  let provider = new UrlbarTestUtils.TestProvider({
    priority: Infinity,
    type: UrlbarUtils.PROVIDER_TYPE.HEURISTIC,
    results: [
      Object.assign(
        new UrlbarResult(
          UrlbarUtils.RESULT_TYPE.URL,
          UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
          {
            title: "Test",
            url: "http://example.com/",
          }
        ),
        {
          heuristic: true,
          autofill: {
            value: autofilledValue,
            selectionStart: searchString.length,
            selectionEnd: autofilledValue.length,
            // Leave out `type` to trigger "other"
          },
        }
      ),
    ],
  });
  UrlbarProvidersManager.registerProvider(provider);

  let histograms = snapshotHistograms();
  await triggerAutofillAndPickResult(searchString, autofilledValue);

  assertSearchTelemetryEmpty(histograms.search_hist);
  assertTelemetryResults(
    histograms,
    "autofill_other",
    0,
    UrlbarTestUtils.SELECTED_RESULT_METHODS.enter
  );

  await PlacesUtils.history.clear();
  UrlbarProvidersManager.unregisterProvider(provider);
});
