/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests interactions with the search mode indicator. See browser_oneOffs.js for
 * more coverage.
 */

const TEST_QUERY = "test string";
const DEFAULT_ENGINE_NAME = "Test";
const SUGGESTIONS_ENGINE_NAME = "searchSuggestionEngine.xml";

let suggestionsEngine;
let defaultEngine;

add_task(async function setup() {
  suggestionsEngine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + SUGGESTIONS_ENGINE_NAME
  );

  let oldDefaultEngine = await Services.search.getDefault();
  defaultEngine = await Services.search.addEngineWithDetails(
    DEFAULT_ENGINE_NAME,
    {
      template: "http://example.com/?search={searchTerms}",
    }
  );
  await Services.search.setDefault(defaultEngine);
  await Services.search.moveEngine(suggestionsEngine, 0);

  registerCleanupFunction(async () => {
    await Services.search.setDefault(oldDefaultEngine);
    await Services.search.removeEngine(defaultEngine);
  });

  SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.separatePrivateDefault.ui.enabled", false],
      ["browser.urlbar.update2", true],
      ["browser.urlbar.update2.oneOffsRefresh", true],
    ],
  });
});

/**
 * Enters search mode by clicking the first one-off.
 * @param {object} window
 */
async function enterSearchMode(window) {
  let oneOffs = UrlbarTestUtils.getOneOffSearchButtons(
    window
  ).getSelectableButtons(true);
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeMouseAtCenter(oneOffs[0], {});
  await searchPromise;
  Assert.ok(UrlbarTestUtils.isPopupOpen(window), "Urlbar view is still open.");
  UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    engineName: oneOffs[0].engine.name,
  });
}

/**
 * Exits search mode.
 * @param {object} window
 * @param {boolean} options.backspace
 *   Exits search mode by backspacing at the beginning of the search string.
 * @param {boolean} options.clickClose
 *   Exits search mode by clicking the close button on the search mode
 *   indicator.
 * @param {boolean} [waitForSearch]
 *   Whether the test should wait for a search after exiting search mode.
 *   Defaults to true.
 * @note One and only one of `backspace` and `clickClose` should be passed
 *       as true.
 */
async function exitSearchMode(
  window,
  { backspace, clickClose, waitForSearch = true }
) {
  if (backspace) {
    let urlbarValue = gURLBar.value;
    gURLBar.selectionStart = gURLBar.selectionEnd = 0;
    if (waitForSearch) {
      let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
      EventUtils.synthesizeKey("KEY_Backspace");
      await searchPromise;
    } else {
      EventUtils.synthesizeKey("KEY_Backspace");
    }
    Assert.equal(gURLBar.value, urlbarValue, "Urlbar value hasn't changed.");
    UrlbarTestUtils.assertSearchMode(window, null);
  } else if (clickClose) {
    // We need to hover the indicator to make the close button clickable in the
    // test.
    let indicator = gURLBar.querySelector("#urlbar-search-mode-indicator");
    EventUtils.synthesizeMouseAtCenter(indicator, { type: "mouseover" });
    let closeButton = gURLBar.querySelector(
      "#urlbar-search-mode-indicator-close"
    );
    if (waitForSearch) {
      let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
      EventUtils.synthesizeMouseAtCenter(closeButton, {});
      await searchPromise;
    } else {
      EventUtils.synthesizeMouseAtCenter(closeButton, {});
    }
    UrlbarTestUtils.assertSearchMode(window, null);
  }
}

async function verifySearchModeResultsAdded(window) {
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    3,
    "There should be three results."
  );
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(
    result.searchParams.engine,
    suggestionsEngine.name,
    "The first result should be a search result for our suggestion engine."
  );
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(
    result.searchParams.suggestion,
    `${TEST_QUERY}foo`,
    "The second result should be a suggestion result."
  );
  Assert.equal(
    result.searchParams.engine,
    suggestionsEngine.name,
    "The second result should be a search result for our suggestion engine."
  );
}

async function verifySearchModeResultsRemoved(window) {
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    1,
    "There should only be one result."
  );
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(
    result.searchParams.engine,
    defaultEngine.name,
    "The first result should be a search result for our default engine."
  );
}

// Tests that the indicator is removed when backspacing at the beginning of
// the search string.
add_task(async function backspace() {
  // View open, with string.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_QUERY,
  });
  await enterSearchMode(window);
  await verifySearchModeResultsAdded(window);
  await exitSearchMode(window, { backspace: true });
  await verifySearchModeResultsRemoved(window);
  Assert.ok(UrlbarTestUtils.isPopupOpen(window), "Urlbar view is open.");

  // View open, no string.
  // Open Top Sites.
  await UrlbarTestUtils.promisePopupOpen(window, () => {
    if (gURLBar.getAttribute("pageproxystate") == "invalid") {
      gURLBar.handleRevert();
    }
    EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
  });
  await enterSearchMode(window);
  await exitSearchMode(window, { backspace: true, waitForSearch: false });
  Assert.ok(UrlbarTestUtils.isPopupOpen(window), "Urlbar view is open.");

  // View closed, with string.
  // Open Top Sites.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_QUERY,
  });
  await enterSearchMode(window);
  await verifySearchModeResultsAdded(window);
  UrlbarTestUtils.promisePopupClose(window);
  await exitSearchMode(window, { backspace: true });
  await verifySearchModeResultsRemoved(window);
  Assert.ok(UrlbarTestUtils.isPopupOpen(window), "Urlbar view is now open.");

  // View closed, no string.
  await UrlbarTestUtils.promisePopupOpen(window, () => {
    if (gURLBar.getAttribute("pageproxystate") == "invalid") {
      gURLBar.handleRevert();
    }
    EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
  });
  await enterSearchMode(window);
  UrlbarTestUtils.promisePopupClose(window);
  await exitSearchMode(window, { backspace: true, waitForSearch: false });
  Assert.ok(
    !UrlbarTestUtils.isPopupOpen(window),
    "Urlbar view is still closed."
  );
});

// Tests the indicator's interaction with the ESC key.
add_task(async function escape() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_QUERY,
  });
  await enterSearchMode(window);
  await verifySearchModeResultsAdded(window);

  EventUtils.synthesizeKey("KEY_Escape");
  Assert.ok(!UrlbarTestUtils.isPopupOpen(window, "UrlbarView is closed."));
  Assert.equal(gURLBar.value, TEST_QUERY, "Urlbar value hasn't changed.");

  let oneOffs = UrlbarTestUtils.getOneOffSearchButtons(
    window
  ).getSelectableButtons(true);
  UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    engineName: oneOffs[0].engine.name,
  });

  EventUtils.synthesizeKey("KEY_Escape");
  Assert.ok(!UrlbarTestUtils.isPopupOpen(window, "UrlbarView is closed."));
  Assert.ok(!gURLBar.value, "Urlbar value is empty.");
  UrlbarTestUtils.assertSearchMode(window, null);
});

// Tests that the indicator is removed when its close button is clicked.
add_task(async function click_close() {
  // Clicking close with the view open.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_QUERY,
  });
  await enterSearchMode(window);
  await verifySearchModeResultsAdded(window);
  await exitSearchMode(window, { clickClose: true });
  await verifySearchModeResultsRemoved(window);

  // Clicking close with the view closed.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_QUERY,
  });
  await enterSearchMode(window);
  UrlbarTestUtils.promisePopupClose(window);
  if (gURLBar.hasAttribute("breakout-extend")) {
    await exitSearchMode(window, { clickClose: true, waitForSearch: false });
  } else {
    // If the Urlbar is not extended when it is closed, do not finish this
    // case. The close button is not clickable when the Urlbar is not
    // extended. This scenario might be encountered on Linux, where
    // prefers-reduced-motion is enabled in automation.
    gURLBar.setSearchMode(null);
  }
});
