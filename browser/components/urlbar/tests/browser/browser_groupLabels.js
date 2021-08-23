/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests group labels in the view.

"use strict";

const SUGGESTIONS_FIRST_PREF = "browser.urlbar.showSearchSuggestionsFirst";
const SUGGESTIONS_PREF = "browser.urlbar.suggest.searches";

const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";
const TEST_ENGINE_2_BASENAME = "searchSuggestionEngine2.xml";
const MAX_RESULTS = UrlbarPrefs.get("maxRichResults");

const TOP_SITES = [
  "http://example-1.com/",
  "http://example-2.com/",
  "http://example-3.com/",
];

const FIREFOX_SUGGEST_LABEL = "Firefox Suggest";

// %s is replaced with the engine name.
const ENGINE_SUGGESTIONS_LABEL = "%s Suggestions";

// Allow more time for Mac machines so they don't time out in verify mode.
if (AppConstants.platform == "macosx") {
  requestLongerTimeout(3);
}

add_task(async function init() {
  Assert.ok(
    UrlbarPrefs.get("showSearchSuggestionsFirst"),
    "Precondition: Search suggestions shown first by default"
  );

  // Add some history.
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
  await UrlbarTestUtils.formHistory.clear();
  await addHistory();

  // Make sure we have some top sites.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.topsites", true],
      ["browser.newtabpage.activity-stream.default.sites", TOP_SITES.join(",")],
    ],
  });
  // Waiting for all top sites to be added intermittently times out, so just
  // wait for any to be added. We're not testing top sites here; we only need
  // the view to open in top-sites mode.
  await updateTopSites(sites => sites && sites.length);

  // Add a mock engine so we don't hit the network.
  await SearchTestUtils.installSearchExtension();
  let oldDefaultEngine = await Services.search.getDefault();
  await Services.search.setDefault(Services.search.getEngineByName("Example"));

  registerCleanupFunction(async () => {
    await PlacesUtils.history.clear();
    Services.search.setDefault(oldDefaultEngine);
  });
});

// The Firefox Suggest label should not appear when the labels pref is disabled.
add_task(async function prefDisabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.groupLabels.enabled", false]],
  });
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test",
  });
  await checkLabels(MAX_RESULTS, {});
  await UrlbarTestUtils.promisePopupClose(window);
  await SpecialPowers.popPrefEnv();
});

// The Firefox Suggest label should not appear when the view shows top sites.
add_task(async function topSites() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });
  await checkLabels(-1, {});
  await UrlbarTestUtils.promisePopupClose(window);
});

// The Firefox Suggest label should appear when the search string is non-empty
// and there are only general results.
add_task(async function general() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test",
  });
  await checkLabels(MAX_RESULTS, {
    1: FIREFOX_SUGGEST_LABEL,
  });
  await UrlbarTestUtils.promisePopupClose(window);
});

// The Firefox Suggest label should appear when the search string is non-empty
// and there are suggestions followed by general results.
add_task(async function suggestionsBeforeGeneral() {
  await withSuggestions(async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "test",
    });
    await checkLabels(MAX_RESULTS, {
      3: FIREFOX_SUGGEST_LABEL,
    });
    await UrlbarTestUtils.promisePopupClose(window);
  });
});

// Both the Firefox Suggest and Suggestions labels should appear when the search
// string is non-empty, general results are shown before suggestions, and there
// are general and suggestion results.
add_task(async function generalBeforeSuggestions() {
  await withSuggestions(async engine => {
    Assert.ok(engine.name, "Engine name is non-empty");
    await SpecialPowers.pushPrefEnv({
      set: [[SUGGESTIONS_FIRST_PREF, false]],
    });
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "test",
    });
    await checkLabels(MAX_RESULTS, {
      1: FIREFOX_SUGGEST_LABEL,
      [MAX_RESULTS - 2]: engineSuggestionsLabel(engine.name),
    });
    await UrlbarTestUtils.promisePopupClose(window);
  });
});

// Neither the Firefox Suggest nor Suggestions label should appear when the
// search string is non-empty, general results are shown before suggestions, and
// there are only suggestion results.
add_task(async function generalBeforeSuggestions_suggestionsOnly() {
  await PlacesUtils.history.clear();

  await withSuggestions(async engine => {
    await SpecialPowers.pushPrefEnv({
      set: [[SUGGESTIONS_FIRST_PREF, false]],
    });
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "test",
    });
    await checkLabels(3, {});
    await UrlbarTestUtils.promisePopupClose(window);
  });

  // Add back history so subsequent tasks run with this test's initial state.
  await addHistory();
});

// The Suggestions label should be updated when the default engine changes.
add_task(async function generalBeforeSuggestions_defaultChanged() {
  // Install both test engines, one after the other. Engine 2 will be the final
  // default engine.
  await withSuggestions(async engine1 => {
    await withSuggestions(async engine2 => {
      Assert.ok(engine2.name, "Engine 2 name is non-empty");
      Assert.notEqual(engine1.name, engine2.name, "Engine names are different");
      Assert.equal(
        Services.search.defaultEngine.name,
        engine2.name,
        "Engine 2 is default"
      );
      await SpecialPowers.pushPrefEnv({
        set: [[SUGGESTIONS_FIRST_PREF, false]],
      });
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: "test",
      });
      await checkLabels(MAX_RESULTS, {
        1: FIREFOX_SUGGEST_LABEL,
        [MAX_RESULTS - 2]: engineSuggestionsLabel(engine2.name),
      });
      await UrlbarTestUtils.promisePopupClose(window);
    }, TEST_ENGINE_2_BASENAME);
  });
});

// The Firefox Suggest label should appear above a suggested-index result when
// the result is the only result with that label.
add_task(async function suggestedIndex_only() {
  // Clear history, add a provider that returns a result with suggestedIndex =
  // -1, set up an engine with suggestions, and start a query. The suggested-
  // index result will be the only result with a label.
  await PlacesUtils.history.clear();

  let index = -1;
  let provider = new SuggestedIndexProvider(index);
  UrlbarProvidersManager.registerProvider(provider);

  await withSuggestions(async engine => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "test",
    });
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 3);
    Assert.equal(
      result.element.row.result.suggestedIndex,
      index,
      "Sanity check: Our suggested-index result is present"
    );
    await checkLabels(4, {
      3: FIREFOX_SUGGEST_LABEL,
    });
    await UrlbarTestUtils.promisePopupClose(window);
  });

  UrlbarProvidersManager.unregisterProvider(provider);

  // Add back history so subsequent tasks run with this test's initial state.
  await addHistory();
});

// The Firefox Suggest label should appear above a suggested-index result when
// the result is the first but not the only result with that label.
add_task(async function suggestedIndex_first() {
  let index = 1;
  let provider = new SuggestedIndexProvider(index);
  UrlbarProvidersManager.registerProvider(provider);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test",
  });
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, index);
  Assert.equal(
    result.element.row.result.suggestedIndex,
    index,
    "Sanity check: Our suggested-index result is present"
  );
  await checkLabels(MAX_RESULTS, {
    [index]: FIREFOX_SUGGEST_LABEL,
  });
  await UrlbarTestUtils.promisePopupClose(window);

  UrlbarProvidersManager.unregisterProvider(provider);
});

// The Firefox Suggest label should not appear above a suggested-index result
// when the result is not the first with that label.
add_task(async function suggestedIndex_notFirst() {
  let index = -1;
  let provider = new SuggestedIndexProvider(index);
  UrlbarProvidersManager.registerProvider(provider);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test",
  });
  let result = await UrlbarTestUtils.getDetailsOfResultAt(
    window,
    MAX_RESULTS + index
  );
  Assert.equal(
    result.element.row.result.suggestedIndex,
    index,
    "Sanity check: Our suggested-index result is present"
  );
  await checkLabels(MAX_RESULTS, {
    1: FIREFOX_SUGGEST_LABEL,
  });
  await UrlbarTestUtils.promisePopupClose(window);

  UrlbarProvidersManager.unregisterProvider(provider);
});

// Labels that appear multiple times but not consecutively should be shown.
add_task(async function repeatLabels() {
  let engineName = Services.search.defaultEngine.name;
  let results = [
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
      { url: "http://example.com/1" }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.SEARCH,
      UrlbarUtils.RESULT_SOURCE.SEARCH,
      { suggestion: "test1", engine: engineName }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
      { url: "http://example.com/2" }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.SEARCH,
      UrlbarUtils.RESULT_SOURCE.SEARCH,
      { suggestion: "test2", engine: engineName }
    ),
  ];

  for (let i = 0; i < results.length; i++) {
    results[i].suggestedIndex = i;
  }

  let provider = new UrlbarTestUtils.TestProvider({
    results,
    priority: Infinity,
  });
  UrlbarProvidersManager.registerProvider(provider);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test",
  });
  await checkLabels(results.length, {
    0: FIREFOX_SUGGEST_LABEL,
    1: engineSuggestionsLabel(engineName),
    2: FIREFOX_SUGGEST_LABEL,
    3: engineSuggestionsLabel(engineName),
  });
  await UrlbarTestUtils.promisePopupClose(window);

  UrlbarProvidersManager.unregisterProvider(provider);
});

// Clicking a row label shouldn't do anything.
add_task(async function clickLabel() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    // Do a search. The mock history added in init() should appear with the
    // Firefox Suggest label at index 1.
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "test",
    });
    await checkLabels(MAX_RESULTS, {
      1: FIREFOX_SUGGEST_LABEL,
    });

    // Check the result at index 2.
    let result2 = await UrlbarTestUtils.getDetailsOfResultAt(window, 2);
    Assert.ok(result2.url, "Result at index 2 has a URL");
    let url2 = result2.url;
    Assert.ok(
      url2.startsWith("http://example.com/"),
      "Result at index 2 is one of our mock history results"
    );

    // Get the row at index 3 and click above it. The click should hit the row
    // at index 2 and load its URL. We do this to make sure our click code
    // here in the test works properly and that performing a similar click
    // relative to index 1 (see below) would hit the row at index 0 if not for
    // the label at index 1.
    let result3 = await UrlbarTestUtils.getDetailsOfResultAt(window, 3);
    let loadPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

    info("Performing click relative to index 3");
    await UrlbarTestUtils.promisePopupClose(window, () =>
      click(result3.element.row, { y: -2 })
    );
    info("Waiting for load after performing click relative to index 3");
    await loadPromise;
    Assert.equal(gBrowser.currentURI.spec, url2, "Loaded URL at index 2");
    // Now do the search again.
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "test",
    });

    await checkLabels(MAX_RESULTS, {
      1: FIREFOX_SUGGEST_LABEL,
    });

    // Check the result at index 1, the one with the label.
    let result1 = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
    Assert.ok(result1.url, "Result at index 1 has a URL");
    let url1 = result1.url;
    Assert.ok(
      url1.startsWith("http://example.com/"),
      "Result at index 1 is one of our mock history results"
    );
    Assert.notEqual(url1, url2, "URLs at indexes 1 and 2 are different");

    // Do a click on the row at index 1 in the same way as before. This time
    // nothing should happen because the click should hit the label, not the
    // row at index 0.
    info("Clicking row label at index 1");
    click(result1.element.row, { y: -2 });
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(r => setTimeout(r, 500));
    Assert.ok(UrlbarTestUtils.isPopupOpen(window), "View remains open");
    Assert.equal(
      gBrowser.currentURI.spec,
      url2,
      "Current URL is still URL from index 2"
    );

    // Now click the main part of the row at index 1. Its URL should load.
    loadPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    let { height } = result1.element.row.getBoundingClientRect();
    info(`Clicking main part of the row at index 1, height=${height}`);
    await UrlbarTestUtils.promisePopupClose(window, () =>
      click(result1.element.row)
    );
    info("Waiting for load after clicking row at index 1");
    await loadPromise;
    Assert.equal(gBrowser.currentURI.spec, url1, "Loaded URL at index 1");
  });
});

/**
 * Provider that returns a suggested-index result.
 */
class SuggestedIndexProvider extends UrlbarTestUtils.TestProvider {
  constructor(suggestedIndex) {
    super({
      results: [
        Object.assign(
          new UrlbarResult(
            UrlbarUtils.RESULT_TYPE.URL,
            UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
            { url: "http://example.com/" }
          ),
          { suggestedIndex }
        ),
      ],
    });
  }
}

async function addHistory() {
  for (let i = 0; i < MAX_RESULTS; i++) {
    await PlacesTestUtils.addVisits("http://example.com/" + i);
  }
}

/**
 * Asserts that each result in the view does or doesn't have a label, depending
 * on `labelsByIndex`.
 *
 * @param {number} resultCount
 *   The expected number of results. Pass -1 to use the max index in
 *   `labelsByIndex` or the actual result count if `labelsByIndex` is empty.
 * @param {object} labelsByIndex
 *   A mapping from indexes to expected labels.
 */
async function checkLabels(resultCount, labelsByIndex) {
  if (resultCount >= 0) {
    Assert.equal(
      UrlbarTestUtils.getResultCount(window),
      resultCount,
      "Expected result count"
    );
  } else {
    // This `else` branch is only necessary because waiting for all top sites to
    // be added intermittently times out. Don't let the test fail for such a
    // dumb reason.
    let indexes = Object.keys(labelsByIndex);
    if (indexes.length) {
      resultCount = indexes.sort((a, b) => b - a)[0] + 1;
    } else {
      resultCount = UrlbarTestUtils.getResultCount(window);
      Assert.greater(resultCount, 0, "Actual result count is > 0");
    }
  }
  for (let i = 0; i < resultCount; i++) {
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    let { row } = result.element;
    let before = getComputedStyle(row, "::before");
    if (labelsByIndex.hasOwnProperty(i)) {
      Assert.equal(
        before.content,
        "attr(label)",
        `::before.content is correct at index ${i}`
      );
      Assert.equal(
        row.getAttribute("label"),
        labelsByIndex[i],
        `Row has correct label at index ${i}`
      );
    } else {
      Assert.equal(
        before.content,
        "none",
        `::before.content is 'none' at index ${i}`
      );
      Assert.ok(
        !row.hasAttribute("label"),
        `Row does not have label attribute at index ${i}`
      );
    }
  }
}

function engineSuggestionsLabel(engineName) {
  return ENGINE_SUGGESTIONS_LABEL.replace("%s", engineName);
}

/**
 * Adds a search engine that provides suggestions, calls your callback, and then
 * remove the engine.
 *
 * @param {function} callback
 *   Your callback function.
 * @param {string} [engineBasename]
 *   The basename of the engine file.
 */
async function withSuggestions(
  callback,
  engineBasename = TEST_ENGINE_BASENAME
) {
  await SpecialPowers.pushPrefEnv({
    set: [[SUGGESTIONS_PREF, true]],
  });
  let engine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + engineBasename
  );
  let oldDefaultEngine = await Services.search.getDefault();
  await Services.search.setDefault(engine);
  try {
    await callback(engine);
  } finally {
    await Services.search.setDefault(oldDefaultEngine);
    await Services.search.removeEngine(engine);
    await SpecialPowers.popPrefEnv();
  }
}

function click(element, { x = undefined, y = undefined } = {}) {
  let { width, height } = element.getBoundingClientRect();
  if (typeof x != "number") {
    x = width / 2;
  }
  if (typeof y != "number") {
    y = height / 2;
  }
  EventUtils.synthesizeMouse(element, x, y, { type: "mousedown" });
  EventUtils.synthesizeMouse(element, x, y, { type: "mouseup" });
}
