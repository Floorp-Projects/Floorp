/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests urlbar autofill telemetry.
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarProviderPreloadedSites:
    "resource:///modules/UrlbarProviderPreloadedSites.sys.mjs",
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
    resultMethodHist: TelemetryTestUtils.getAndClearHistogram(
      "FX_URLBAR_SELECTED_RESULT_METHOD"
    ),
    search_hist: TelemetryTestUtils.getAndClearKeyedHistogram("SEARCH_COUNTS"),
  };
}

function assertTelemetryResults(histograms, type, index, method) {
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
 * @param {string} unpickResult
 *   Optional: If true, do not pick any result. Default value is false.
 * @param {string} urlToSelect
 *   Optional: If want to select result except autofill, pass the URL.
 */
async function triggerAutofillAndPickResult(
  searchString,
  autofilledValue,
  unpickResult = false,
  urlToSelect = null
) {
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

    if (urlToSelect) {
      for (let row = 0; row < UrlbarTestUtils.getResultCount(window); row++) {
        const result = await UrlbarTestUtils.getDetailsOfResultAt(window, row);
        if (result.url === urlToSelect) {
          UrlbarTestUtils.setSelectedRowIndex(window, row);
          break;
        }
      }
    }

    if (unpickResult) {
      // Close popup without any action.
      await UrlbarTestUtils.promisePopupClose(window);
      return;
    }

    let loadPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    EventUtils.synthesizeKey("KEY_Enter");
    await loadPromise;

    let url;
    if (urlToSelect) {
      url = urlToSelect;
    } else {
      url = autofilledValue.includes(":")
        ? autofilledValue
        : "http://" + autofilledValue;
    }
    Assert.equal(gBrowser.currentURI.spec, url, "Loaded URL is correct");
  });
}

function createOtherAutofillProvider(searchString, autofilledValue) {
  return new UrlbarTestUtils.TestProvider({
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
}

// Allow more time for Mac machines so they don't time out in verify mode.
if (AppConstants.platform == "macosx") {
  requestLongerTimeout(3);
}

add_setup(async function() {
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesTestUtils.clearInputHistory();

  // Enable local telemetry recording for the duration of the tests.
  const originalCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;

  registerCleanupFunction(async () => {
    Services.telemetry.canRecordExtended = originalCanRecord;
    await PlacesTestUtils.clearInputHistory();
    await PlacesUtils.history.clear();
  });
});

// Checks adaptive history, origin, and URL autofill.
add_task(async function history() {
  const testData = [
    {
      useAdaptiveHistory: true,
      visitHistory: ["http://example.com/test"],
      inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
      userInput: "ex",
      autofilled: "example.com/",
      expected: "autofill_origin",
    },
    {
      useAdaptiveHistory: true,
      visitHistory: ["http://example.com/test"],
      inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
      userInput: "exa",
      autofilled: "example.com/test",
      expected: "autofill_adaptive",
    },
    {
      useAdaptiveHistory: true,
      visitHistory: ["http://example.com/test"],
      inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
      userInput: "exam",
      autofilled: "example.com/test",
      expected: "autofill_adaptive",
    },
    {
      useAdaptiveHistory: true,
      visitHistory: ["http://example.com/test"],
      inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
      userInput: "example.com",
      autofilled: "example.com/test",
      expected: "autofill_adaptive",
    },
    {
      useAdaptiveHistory: true,
      visitHistory: ["http://example.com/test"],
      inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
      userInput: "example.com/",
      autofilled: "example.com/test",
      expected: "autofill_adaptive",
    },
    {
      useAdaptiveHistory: true,
      visitHistory: ["http://example.com/test"],
      inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
      userInput: "example.com/test",
      autofilled: "example.com/test",
      expected: "autofill_adaptive",
    },
    {
      useAdaptiveHistory: true,
      visitHistory: ["http://example.com/test", "http://example.org/test"],
      inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
      userInput: "example.org",
      autofilled: "example.org/",
      expected: "autofill_origin",
    },
    {
      useAdaptiveHistory: true,
      visitHistory: ["http://example.com/test", "http://example.com/test/url"],
      inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
      userInput: "example.com/test/",
      autofilled: "example.com/test/",
      expected: "autofill_url",
    },
    {
      useAdaptiveHistory: true,
      visitHistory: [{ uri: "http://example.com/test" }],
      inputHistory: [
        { uri: "http://example.com/test", input: "http://example.com/test" },
      ],
      userInput: "http://example.com/test",
      autofilled: "http://example.com/test",
      expected: "autofill_adaptive",
    },
    {
      useAdaptiveHistory: false,
      visitHistory: [{ uri: "http://example.com/test" }],
      inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
      userInput: "example",
      autofilled: "example.com/",
      expected: "autofill_origin",
    },
    {
      useAdaptiveHistory: false,
      visitHistory: [{ uri: "http://example.com/test" }],
      inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
      userInput: "example.com/te",
      autofilled: "example.com/test",
      expected: "autofill_url",
    },
  ];

  for (const {
    useAdaptiveHistory,
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

    UrlbarPrefs.set("autoFill.adaptiveHistory.enabled", useAdaptiveHistory);

    await triggerAutofillAndPickResult(userInput, autofilled);

    assertSearchTelemetryEmpty(histograms.search_hist);
    assertTelemetryResults(
      histograms,
      expected,
      0,
      UrlbarTestUtils.SELECTED_RESULT_METHODS.enter
    );

    UrlbarPrefs.clear("autoFill.adaptiveHistory.enabled");
    await PlacesTestUtils.clearInputHistory();
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
  let provider = createOtherAutofillProvider(searchString, autofilledValue);
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

// Checks impression telemetry.
add_task(async function impression() {
  const testData = [
    {
      description: "Adaptive history autofill and pick it",
      useAdaptiveHistory: true,
      visitHistory: ["http://example.com/first", "http://example.com/second"],
      inputHistory: [{ uri: "http://example.com/first", input: "exa" }],
      userInput: "exa",
      autofilled: "example.com/first",
      expected: "autofill_adaptive",
    },
    {
      description: "Adaptive history autofill but pick another result",
      useAdaptiveHistory: true,
      visitHistory: ["http://example.com/first", "http://example.com/second"],
      inputHistory: [{ uri: "http://example.com/first", input: "exa" }],
      userInput: "exa",
      urlToSelect: "http://example.com/second",
      autofilled: "example.com/first",
      expected: "autofill_adaptive",
    },
    {
      description: "Adaptive history autofill but not pick any result",
      unpickResult: true,
      useAdaptiveHistory: true,
      visitHistory: ["http://example.com/first", "http://example.com/second"],
      inputHistory: [{ uri: "http://example.com/first", input: "exa" }],
      userInput: "exa",
      autofilled: "example.com/first",
    },
    {
      description: "Origin autofill and pick it",
      visitHistory: ["http://example.com/first", "http://example.com/second"],
      userInput: "exa",
      autofilled: "example.com/",
      expected: "autofill_origin",
    },
    {
      description: "Origin autofill but pick another result",
      visitHistory: ["http://example.com/first", "http://example.com/second"],
      userInput: "exa",
      urlToSelect: "http://example.com/second",
      autofilled: "example.com/",
      expected: "autofill_origin",
    },
    {
      description: "Origin autofill but not pick any result",
      unpickResult: true,
      visitHistory: ["http://example.com/first", "http://example.com/second"],
      userInput: "exa",
      autofilled: "example.com/",
    },
    {
      description: "URL autofill and pick it",
      visitHistory: ["http://example.com/first", "http://example.com/second"],
      userInput: "example.com/",
      autofilled: "example.com/",
      expected: "autofill_url",
    },
    {
      description: "URL autofill but pick another result",
      visitHistory: ["http://example.com/first", "http://example.com/second"],
      userInput: "example.com/",
      urlToSelect: "http://example.com/second",
      autofilled: "example.com/",
      expected: "autofill_url",
    },
    {
      description: "URL autofill but not pick any result",
      unpickResult: true,
      visitHistory: ["http://example.com/first", "http://example.com/second"],
      userInput: "example.com/",
      autofilled: "example.com/",
    },
    {
      description: "about page autofill and pick it",
      userInput: "about:a",
      autofilled: "about:about",
      expected: "autofill_about",
    },
    {
      description: "about page autofill but pick another result",
      userInput: "about:a",
      urlToSelect: "about:addons",
      autofilled: "about:about",
      expected: "autofill_about",
    },
    {
      description: "about page autofill but not pick any result",
      unpickResult: true,
      userInput: "about:a",
      autofilled: "about:about",
    },
    {
      description: "Preloaded site autofill and pick it",
      usePreloadedSite: true,
      preloadedSites: [["http://example.com/", "Example"]],
      userInput: "exa",
      autofilled: "example.com/",
      expected: "autofill_preloaded",
    },
    {
      description: "Preloaded site autofill but not pick any result",
      unpickResult: true,
      usePreloadedSite: true,
      preloadedSites: [["http://example.com/", "Example"]],
      userInput: "exa",
      autofilled: "example.com/",
    },
    {
      description: "Other provider's autofill and pick it",
      useOtherProvider: true,
      userInput: "example",
      autofilled: "example.com/",
      expected: "autofill_other",
    },
    {
      description: "Other provider's autofill but not pick any result",
      unpickResult: true,
      useOtherProvider: true,
      userInput: "example",
      autofilled: "example.com/",
    },
  ];

  for (const {
    description,
    useAdaptiveHistory = false,
    usePreloadedSite = false,
    useOtherProvider = false,
    unpickResult = false,
    visitHistory,
    inputHistory,
    preloadedSites,
    userInput,
    select,
    autofilled,
    expected,
  } of testData) {
    info(description);

    UrlbarPrefs.set("autoFill.adaptiveHistory.enabled", useAdaptiveHistory);
    if (usePreloadedSite) {
      UrlbarPrefs.set("usepreloadedtopurls.enabled", true);
      UrlbarPrefs.set("usepreloadedtopurls.expire_days", 100);
    }
    let otherProvider;
    if (useOtherProvider) {
      otherProvider = createOtherAutofillProvider(userInput, autofilled);
      UrlbarProvidersManager.registerProvider(otherProvider);
    }

    if (visitHistory) {
      await PlacesTestUtils.addVisits(visitHistory);
    }
    if (inputHistory) {
      for (const { uri, input } of inputHistory) {
        await UrlbarUtils.addToInputHistory(uri, input);
      }
    }
    if (preloadedSites) {
      UrlbarProviderPreloadedSites.populatePreloadedSiteStorage(preloadedSites);
    }

    await triggerAutofillAndPickResult(
      userInput,
      autofilled,
      unpickResult,
      select
    );

    const scalars = TelemetryTestUtils.getProcessScalars("parent", false, true);
    if (unpickResult) {
      TelemetryTestUtils.assertScalarUnset(
        scalars,
        "urlbar.impression.autofill_adaptive"
      );
      TelemetryTestUtils.assertScalarUnset(
        scalars,
        "urlbar.impression.autofill_origin"
      );
      TelemetryTestUtils.assertScalarUnset(
        scalars,
        "urlbar.impression.autofill_url"
      );
      TelemetryTestUtils.assertScalarUnset(
        scalars,
        "urlbar.impression.autofill_about"
      );
    } else {
      TelemetryTestUtils.assertScalar(
        scalars,
        `urlbar.impression.${expected}`,
        1
      );
    }

    UrlbarPrefs.clear("autoFill.adaptiveHistory.enabled");
    UrlbarPrefs.clear("usepreloadedtopurls.enabled");
    UrlbarPrefs.clear("usepreloadedtopurls.expire_days");

    if (otherProvider) {
      UrlbarProvidersManager.unregisterProvider(otherProvider);
    }

    await PlacesTestUtils.clearInputHistory();
    await PlacesUtils.history.clear();
  }
});
