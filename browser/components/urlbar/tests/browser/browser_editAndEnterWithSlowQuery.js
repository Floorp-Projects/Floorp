/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test when a user enters a different URL than the result being selected.

"use strict";

const ORIGINAL_CHUNK_RESULTS_DELAY =
  UrlbarProvidersManager.CHUNK_RESULTS_DELAY_MS;

add_setup(async function setup() {
  let suggestionsEngine = await SearchTestUtils.promiseNewSearchEngine({
    url: getRootDirectory(gTestPath) + "searchSuggestionEngine.xml",
  });
  await SearchTestUtils.installSearchExtension(
    {
      name: "Test",
      keyword: "@test",
    },
    { setAsDefault: true }
  );
  await Services.search.moveEngine(suggestionsEngine, 0);

  registerCleanupFunction(async () => {
    UrlbarProvidersManager.CHUNK_RESULTS_DELAY_MS =
      ORIGINAL_CHUNK_RESULTS_DELAY;
    UrlbarPrefs.clear("delay");
  });

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.quickactions", false],
      ["browser.urlbar.trimHttps", false],
      [
        "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features",
        false,
      ],
    ],
  });
});

add_task(async function test_url_type() {
  const testCases = [
    {
      testURL: "https://example.com/123",
      displayedURL: "https://example.com/123",
      trimURLs: true,
    },
    {
      testURL: "https://example.com/123",
      displayedURL: "https://example.com/123",
      trimURLs: false,
    },
    {
      // eslint-disable-next-line @microsoft/sdl/no-insecure-url
      testURL: "http://example.com/123",
      displayedURL: "example.com/123",
      trimURLs: true,
    },
    {
      // eslint-disable-next-line @microsoft/sdl/no-insecure-url
      testURL: "http://example.com/123",
      // eslint-disable-next-line @microsoft/sdl/no-insecure-url
      displayedURL: "http://example.com/123",
      trimURLs: false,
    },
  ];

  for (const { testURL, displayedURL, trimURLs } of testCases) {
    info("Setup: " + JSON.stringify({ testURL, displayedURL, trimURLs }));
    await SpecialPowers.pushPrefEnv({
      set: [["browser.urlbar.trimURLs", trimURLs]],
    });
    await PlacesTestUtils.addVisits([testURL]);
    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();

    info("Show results");
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "exa",
      fireInputEvent: true,
    });

    info("Find target result");
    let targetRowIndex = await findTargetRowIndex(
      result =>
        result.type == UrlbarUtils.RESULT_TYPE.URL && result.url == testURL
    );

    info("Select a visit suggestion");
    UrlbarTestUtils.setSelectedRowIndex(window, targetRowIndex);
    Assert.equal(window.gURLBar.value, displayedURL);

    info("Change the delay time to avoid updating results");
    const DELAY = 10000;
    UrlbarProvidersManager.CHUNK_RESULTS_DELAY_MS = DELAY;
    UrlbarPrefs.set("delay", DELAY);

    info("Edit text on the URL bar");
    window.gURLBar.setSelectionRange(
      Number.MAX_SAFE_INTEGER,
      Number.MAX_SAFE_INTEGER
    );
    EventUtils.synthesizeKey("KEY_Backspace");
    Assert.ok(gURLBar.valueIsTyped);
    Assert.equal(UrlbarTestUtils.getSelectedRowIndex(window), targetRowIndex);
    let selectedResult = UrlbarTestUtils.getSelectedRow(window).result;
    Assert.equal(selectedResult.type, UrlbarUtils.RESULT_TYPE.URL);

    info("Enter before updating");
    let loadingURL = testURL.substring(0, testURL.length - 1);
    let onLoad = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser,
      false,
      loadingURL
    );
    EventUtils.synthesizeKey("KEY_Enter");
    await onLoad;
    Assert.equal(gBrowser.currentURI.spec, loadingURL);

    info("Clean up");
    await PlacesUtils.history.clear();
    UrlbarProvidersManager.CHUNK_RESULTS_DELAY_MS =
      ORIGINAL_CHUNK_RESULTS_DELAY;
    UrlbarPrefs.clear("delay");
    await SpecialPowers.popPrefEnv();
  }
});

add_task(async function test_search_type() {
  info("Show results");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "123",
    fireInputEvent: true,
  });
  await UrlbarTestUtils.enterSearchMode(window);

  info("Find target result");
  let targetRowIndex = await findTargetRowIndex(
    result =>
      result.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
      result.url == "http://mochi.test:8888/?terms=123foo"
  );

  info("Select a search suggestion");
  UrlbarTestUtils.setSelectedRowIndex(window, targetRowIndex);
  Assert.equal(window.gURLBar.value, "123foo");

  info("Change the delay time to avoid updating results");
  const DELAY = 10000;
  UrlbarProvidersManager.CHUNK_RESULTS_DELAY_MS = DELAY;
  UrlbarPrefs.set("delay", DELAY);

  info("Edit text on the URL bar");
  window.gURLBar.setSelectionRange(
    Number.MAX_SAFE_INTEGER,
    Number.MAX_SAFE_INTEGER
  );
  EventUtils.synthesizeKey("KEY_Backspace");
  Assert.ok(gURLBar.valueIsTyped);
  Assert.equal(UrlbarTestUtils.getSelectedRowIndex(window), targetRowIndex);
  let selectedResult = UrlbarTestUtils.getSelectedRow(window).result;
  Assert.equal(selectedResult.type, UrlbarUtils.RESULT_TYPE.SEARCH);

  info("Enter before updating");
  let loadingURL = "http://mochi.test:8888/?terms=123fo";
  let onLoad = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    loadingURL
  );
  EventUtils.synthesizeKey("KEY_Enter");
  await onLoad;
  Assert.equal(gBrowser.currentURI.spec, loadingURL);

  info("Clean up");
  await PlacesUtils.history.clear();
  UrlbarProvidersManager.CHUNK_RESULTS_DELAY_MS = ORIGINAL_CHUNK_RESULTS_DELAY;
  UrlbarPrefs.clear("delay");
});

add_task(async function test_keyword_type() {
  info("Setup");
  await PlacesUtils.keywords.insert({
    keyword: "keyword",
    url: "https://example.com/?q=%s",
  });

  info("Show results");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "keyword 123",
    fireInputEvent: true,
  });

  info("Find target result");
  let targetRowIndex = await findTargetRowIndex(
    result =>
      result.type == UrlbarUtils.RESULT_TYPE.KEYWORD &&
      result.url == "https://example.com/?q=123"
  );

  info("Select a search suggestion");
  UrlbarTestUtils.setSelectedRowIndex(window, targetRowIndex);
  Assert.equal(window.gURLBar.value, "keyword 123");

  info("Change the delay time to avoid updating results");
  const DELAY = 10000;
  UrlbarProvidersManager.CHUNK_RESULTS_DELAY_MS = DELAY;
  UrlbarPrefs.set("delay", DELAY);

  info("Edit text on the URL bar");
  window.gURLBar.setSelectionRange(
    Number.MAX_SAFE_INTEGER,
    Number.MAX_SAFE_INTEGER
  );
  EventUtils.synthesizeKey("KEY_Backspace");
  Assert.ok(gURLBar.valueIsTyped);
  Assert.equal(UrlbarTestUtils.getSelectedRowIndex(window), targetRowIndex);
  let selectedResult = UrlbarTestUtils.getSelectedRow(window).result;
  Assert.equal(selectedResult.type, UrlbarUtils.RESULT_TYPE.KEYWORD);

  info("Enter before updating");
  let loadingURL = "https://example.com/?q=12";
  let onLoad = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    loadingURL
  );
  EventUtils.synthesizeKey("KEY_Enter");
  await onLoad;
  Assert.equal(gBrowser.currentURI.spec, loadingURL);

  info("Clean up");
  await PlacesUtils.history.clear();
  await PlacesUtils.keywords.remove("keyword");
  UrlbarProvidersManager.CHUNK_RESULTS_DELAY_MS = ORIGINAL_CHUNK_RESULTS_DELAY;
  UrlbarPrefs.clear("delay");
});

add_task(async function test_dynamic_type() {
  info("Setup");
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.unitConversion.enabled", true]],
  });

  info("Show results");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "12 cm to mm",
    fireInputEvent: true,
  });

  info("Find target result");
  let targetRowIndex = await findTargetRowIndex(
    result => result.type == UrlbarUtils.RESULT_TYPE.DYNAMIC
  );

  info("Select a dynamic suggestion");
  UrlbarTestUtils.setSelectedRowIndex(window, targetRowIndex);
  Assert.equal(window.gURLBar.value, "12 cm to mm");

  info("Change the delay time to avoid updating results");
  const DELAY = 10000;
  UrlbarProvidersManager.CHUNK_RESULTS_DELAY_MS = DELAY;
  UrlbarPrefs.set("delay", DELAY);

  info("Edit text on the URL bar");
  window.gURLBar.setSelectionRange(
    Number.MAX_SAFE_INTEGER,
    Number.MAX_SAFE_INTEGER
  );
  EventUtils.synthesizeKey("KEY_Backspace");
  Assert.ok(gURLBar.valueIsTyped);
  Assert.equal(UrlbarTestUtils.getSelectedRowIndex(window), targetRowIndex);
  let selectedResult = UrlbarTestUtils.getSelectedRow(window).result;
  Assert.equal(selectedResult.type, UrlbarUtils.RESULT_TYPE.DYNAMIC);

  info("Enter before updating");
  // TODO: We need to show the dynamic result with different word here.
  let loadingURL = "https://example.com/?q=12+cm+to+m";
  let onLoad = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    loadingURL
  );
  EventUtils.synthesizeKey("KEY_Enter");
  await onLoad;
  Assert.equal(gBrowser.currentURI.spec, loadingURL);

  info("Clean up");
  await PlacesUtils.history.clear();
  await SpecialPowers.popPrefEnv();
  UrlbarProvidersManager.CHUNK_RESULTS_DELAY_MS = ORIGINAL_CHUNK_RESULTS_DELAY;
  UrlbarPrefs.clear("delay");
});

add_task(async function test_omnibox_type() {
  info("Setup");
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
      omnibox: {
        keyword: "omnibox",
      },
    },
    background() {
      /* global browser */
      browser.omnibox.setDefaultSuggestion({
        description: "doit",
      });
      browser.omnibox.onInputEntered.addListener(text => {
        browser.tabs.update({ url: `https://example.com/${text}` });
      });
      browser.omnibox.onInputChanged.addListener((text, suggest) => {
        suggest([]);
      });
    },
  });
  await extension.startup();

  info("Show results");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "omnibox 123",
    fireInputEvent: true,
  });

  info("Find target result");
  let targetRowIndex = await findTargetRowIndex(
    result => result.type == UrlbarUtils.RESULT_TYPE.OMNIBOX
  );

  info("Select an omnibox suggestion");
  UrlbarTestUtils.setSelectedRowIndex(window, targetRowIndex);
  Assert.equal(window.gURLBar.value, "omnibox 123");

  info("Change the delay time to avoid updating results");
  const DELAY = 10000;
  UrlbarProvidersManager.CHUNK_RESULTS_DELAY_MS = DELAY;
  UrlbarPrefs.set("delay", DELAY);

  info("Edit text on the URL bar");
  window.gURLBar.setSelectionRange(
    Number.MAX_SAFE_INTEGER,
    Number.MAX_SAFE_INTEGER
  );
  EventUtils.synthesizeKey("KEY_Backspace");
  Assert.ok(gURLBar.valueIsTyped);
  Assert.equal(UrlbarTestUtils.getSelectedRowIndex(window), targetRowIndex);
  let selectedResult = UrlbarTestUtils.getSelectedRow(window).result;
  Assert.equal(selectedResult.type, UrlbarUtils.RESULT_TYPE.OMNIBOX);
  Assert.ok(selectedResult.heuristic);

  info("Enter before updating");
  // As this result is heuristic, should pick as it is.
  let loadingURL = "https://example.com/123";
  let onLoad = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    loadingURL
  );
  EventUtils.synthesizeKey("KEY_Enter");
  await onLoad;
  Assert.equal(gBrowser.currentURI.spec, loadingURL);

  info("Clean up");
  await PlacesUtils.history.clear();
  await extension.unload();
  UrlbarProvidersManager.CHUNK_RESULTS_DELAY_MS = ORIGINAL_CHUNK_RESULTS_DELAY;
  UrlbarPrefs.clear("delay");
});

add_task(async function test_heuristic() {
  const testCases = [
    {
      testResult: new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        { url: "https://example.com/123" }
      ),
      loadingURL: "https://example.com/123",
      displayedValue: "https://example.com/123",
    },
    {
      testResult: new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.SEARCH,
        UrlbarUtils.RESULT_SOURCE.SEARCH,
        {
          engine: Services.search.defaultEngine.name,
          query: "heuristic_search",
        }
      ),
      loadingURL: "https://example.com/?q=heuristic_search",
      displayedValue: "heuristic_search",
    },
  ];

  for (const { testResult, loadingURL, displayedValue } of testCases) {
    info("Setup: " + JSON.stringify(testResult));
    testResult.heuristic = true;
    let provider = new UrlbarTestUtils.TestProvider({
      results: [testResult],
      name: "TestProviderHeuristic",
      priority: Infinity,
    });
    UrlbarProvidersManager.registerProvider(provider);

    info("Show results");
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "any query",
      fireInputEvent: true,
    });

    info("Select a visit suggestion");
    const targetRowIndex = 0;
    UrlbarTestUtils.setSelectedRowIndex(window, targetRowIndex);
    Assert.equal(window.gURLBar.value, displayedValue);

    info("Change the delay time to avoid updating results");
    const DELAY = 10000;
    UrlbarProvidersManager.CHUNK_RESULTS_DELAY_MS = DELAY;
    UrlbarPrefs.set("delay", DELAY);

    info("Edit text on the URL bar");
    window.gURLBar.setSelectionRange(
      Number.MAX_SAFE_INTEGER,
      Number.MAX_SAFE_INTEGER
    );
    EventUtils.synthesizeKey("KEY_Backspace");
    Assert.ok(gURLBar.valueIsTyped);
    Assert.equal(UrlbarTestUtils.getSelectedRowIndex(window), targetRowIndex);
    let selectedResult = UrlbarTestUtils.getSelectedRow(window).result;
    Assert.equal(selectedResult, testResult);
    Assert.equal(
      window.gURLBar.value,
      displayedValue.substring(0, displayedValue.length - 1)
    );

    info("Enter before updating");
    let spy = sinon.spy(UrlbarUtils, "getHeuristicResultFor");
    let onLoad = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser,
      false,
      loadingURL
    );
    EventUtils.synthesizeKey("KEY_Enter");
    await onLoad;
    Assert.equal(gBrowser.currentURI.spec, loadingURL);
    spy.restore();
    Assert.ok(!spy.called, "getHeuristicResultFor should not be called");

    info("Clean up");
    UrlbarProvidersManager.unregisterProvider(provider);
    UrlbarProvidersManager.CHUNK_RESULTS_DELAY_MS =
      ORIGINAL_CHUNK_RESULTS_DELAY;
    UrlbarPrefs.clear("delay");
  }
});

async function findTargetRowIndex(finder) {
  for (
    let i = 0, count = UrlbarTestUtils.getResultCount(window);
    i < count;
    i++
  ) {
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    if (finder(result)) {
      return i;
    }
  }

  throw new Error("Target not found");
}
