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

// These need to have different domains because otherwise new tab and/or
// activity stream collapses them.
const TOP_SITES_URLS = [
  "http://top-site-0.com/",
  "http://top-site-1.com/",
  "http://top-site-2.com/",
];

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

  // Set our top sites.
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.newtabpage.activity-stream.default.sites",
        TOP_SITES_URLS.join(","),
      ],
    ],
  });
  await updateTopSites(sites =>
    ObjectUtils.deepEqual(
      sites.map(s => s.url),
      TOP_SITES_URLS
    )
  );

  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.separatePrivateDefault.ui.enabled", false]],
  });
});

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

async function verifyTopSitesResultsAdded(window) {
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    TOP_SITES_URLS.length,
    "Expected number of top sites results"
  );
  for (let i = 0; i < TOP_SITES_URLS; i++) {
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    Assert.equal(
      result.url,
      TOP_SITES_URLS[i],
      `Expected top sites result URL at index ${i}`
    );
  }
}

// Tests that the indicator is removed when backspacing at the beginning of
// the search string.
add_task(async function backspace() {
  // View open, with string.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_QUERY,
  });
  await UrlbarTestUtils.enterSearchMode(window);
  await verifySearchModeResultsAdded(window);
  await UrlbarTestUtils.exitSearchMode(window, { backspace: true });
  await verifySearchModeResultsRemoved(window);
  Assert.ok(UrlbarTestUtils.isPopupOpen(window), "Urlbar view is open.");

  // View open, no string (i.e., top sites).
  await UrlbarTestUtils.promisePopupOpen(window, () => {
    if (gURLBar.getAttribute("pageproxystate") == "invalid") {
      gURLBar.handleRevert();
    }
    EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
  });
  await UrlbarTestUtils.enterSearchMode(window);
  await UrlbarTestUtils.exitSearchMode(window, { backspace: true });
  Assert.ok(UrlbarTestUtils.isPopupOpen(window), "Urlbar view is open.");
  await verifyTopSitesResultsAdded(window);
  await UrlbarTestUtils.promisePopupClose(window);

  // View closed, with string.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_QUERY,
  });
  await UrlbarTestUtils.enterSearchMode(window);
  await verifySearchModeResultsAdded(window);
  await UrlbarTestUtils.promisePopupClose(window);
  await UrlbarTestUtils.exitSearchMode(window, { backspace: true });
  await verifySearchModeResultsRemoved(window);
  Assert.ok(UrlbarTestUtils.isPopupOpen(window), "Urlbar view is now open.");

  // View closed, no string (i.e., top sites).
  await UrlbarTestUtils.promisePopupOpen(window, () => {
    if (gURLBar.getAttribute("pageproxystate") == "invalid") {
      gURLBar.handleRevert();
    }
    EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
  });
  await UrlbarTestUtils.enterSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window);
  await UrlbarTestUtils.exitSearchMode(window, { backspace: true });
  Assert.ok(UrlbarTestUtils.isPopupOpen(window), "Urlbar view is open.");
  await verifyTopSitesResultsAdded(window);
  await UrlbarTestUtils.promisePopupClose(window);
});

add_task(async function escapeOnInitialPage() {
  info("Tests the indicator's interaction with the ESC key");

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_QUERY,
  });
  await UrlbarTestUtils.enterSearchMode(window);
  await verifySearchModeResultsAdded(window);

  EventUtils.synthesizeKey("KEY_Escape");
  Assert.ok(!UrlbarTestUtils.isPopupOpen(window, "UrlbarView is closed."));
  Assert.equal(gURLBar.value, TEST_QUERY, "Urlbar value hasn't changed.");

  let oneOffs = UrlbarTestUtils.getOneOffSearchButtons(
    window
  ).getSelectableButtons(true);
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: oneOffs[0].engine.name,
    entry: "oneoff",
  });

  EventUtils.synthesizeKey("KEY_Escape");
  Assert.ok(!UrlbarTestUtils.isPopupOpen(window, "UrlbarView is closed."));
  Assert.ok(!gURLBar.value, "Urlbar value is empty.");
  await UrlbarTestUtils.assertSearchMode(window, null);
});

add_task(async function escapeOnBrowsingPage() {
  info("Tests the indicator's interaction with the ESC key on browsing page");

  await BrowserTestUtils.withNewTab("http://example.com", async browser => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: TEST_QUERY,
    });
    await UrlbarTestUtils.enterSearchMode(window);
    await verifySearchModeResultsAdded(window);

    EventUtils.synthesizeKey("KEY_Escape");
    Assert.ok(!UrlbarTestUtils.isPopupOpen(window, "UrlbarView is closed."));
    Assert.equal(gURLBar.value, TEST_QUERY, "Urlbar value hasn't changed.");

    const oneOffs = UrlbarTestUtils.getOneOffSearchButtons(
      window
    ).getSelectableButtons(true);
    await UrlbarTestUtils.assertSearchMode(window, {
      engineName: oneOffs[0].engine.name,
      entry: "oneoff",
    });

    EventUtils.synthesizeKey("KEY_Escape");
    Assert.ok(!UrlbarTestUtils.isPopupOpen(window, "UrlbarView is closed."));
    Assert.equal(
      gURLBar.value,
      "example.com",
      "Urlbar value indicates the browsing page."
    );
    await UrlbarTestUtils.assertSearchMode(window, null);
  });
});

// Tests that the indicator is removed when its close button is clicked.
add_task(async function click_close() {
  // Clicking close with the view open, with string.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_QUERY,
  });
  await UrlbarTestUtils.enterSearchMode(window);
  await verifySearchModeResultsAdded(window);
  await UrlbarTestUtils.exitSearchMode(window, { clickClose: true });
  await verifySearchModeResultsRemoved(window);
  Assert.ok(UrlbarTestUtils.isPopupOpen(window), "Urlbar view is open.");
  await UrlbarTestUtils.promisePopupClose(window);

  // Clicking close with the view open, no string (i.e., top sites).
  await UrlbarTestUtils.promisePopupOpen(window, () => {
    if (gURLBar.getAttribute("pageproxystate") == "invalid") {
      gURLBar.handleRevert();
    }
    EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
  });
  await UrlbarTestUtils.enterSearchMode(window);
  await UrlbarTestUtils.exitSearchMode(window, { clickClose: true });
  Assert.ok(UrlbarTestUtils.isPopupOpen(window), "Urlbar view is open.");
  await verifyTopSitesResultsAdded(window);
  await UrlbarTestUtils.promisePopupClose(window);

  // Clicking close with the view closed, with string.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_QUERY,
  });
  await UrlbarTestUtils.enterSearchMode(window);
  await verifySearchModeResultsAdded(window);
  await UrlbarTestUtils.promisePopupClose(window);
  await UrlbarTestUtils.exitSearchMode(window, {
    clickClose: true,
    waitForSearch: false,
  });
  Assert.ok(!UrlbarTestUtils.isPopupOpen(window), "Urlbar view is closed.");

  // Clicking close with the view closed, no string (i.e., top sites).
  await UrlbarTestUtils.promisePopupOpen(window, () => {
    if (gURLBar.getAttribute("pageproxystate") == "invalid") {
      gURLBar.handleRevert();
    }
    EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
  });
  await UrlbarTestUtils.enterSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window);
  await UrlbarTestUtils.exitSearchMode(window, {
    clickClose: true,
    waitForSearch: false,
  });
  Assert.ok(!UrlbarTestUtils.isPopupOpen(window), "Urlbar view is closed.");
});

// Tests that Accel+K enters search mode with the default engine. Also tests
// that Accel+K highlights the typed search string.
add_task(async function keyboard_shortcut() {
  const query = "test query";
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: query,
  });
  await UrlbarTestUtils.assertSearchMode(window, null);
  Assert.equal(
    gURLBar.selectionStart,
    gURLBar.selectionEnd,
    "The search string is not highlighted."
  );
  EventUtils.synthesizeKey("k", { accelKey: true });
  await UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    engineName: defaultEngine.name,
    entry: "shortcut",
  });
  Assert.equal(gURLBar.value, query, "The search string was not cleared.");
  Assert.equal(gURLBar.selectionStart, 0);
  Assert.equal(
    gURLBar.selectionEnd,
    query.length,
    "The search string is highlighted."
  );
  await UrlbarTestUtils.exitSearchMode(window, {
    clickClose: true,
    waitForSearch: false,
  });
  await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
});

// Tests that the Tools:Search menu item enters search mode with the default
// engine. Also tests that Tools:Search highlights the typed search string.
add_task(async function menubar_item() {
  const query = "test query 2";
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: query,
  });
  await UrlbarTestUtils.assertSearchMode(window, null);
  Assert.equal(
    gURLBar.selectionStart,
    gURLBar.selectionEnd,
    "The search string is not highlighted."
  );
  let command = window.document.getElementById("Tools:Search");
  command.doCommand();
  await UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    engineName: defaultEngine.name,
    entry: "shortcut",
  });
  Assert.equal(gURLBar.value, query, "The search string was not cleared.");
  Assert.equal(gURLBar.selectionStart, 0);
  Assert.equal(
    gURLBar.selectionEnd,
    query.length,
    "The search string is highlighted."
  );
  await UrlbarTestUtils.exitSearchMode(window, {
    clickClose: true,
    waitForSearch: false,
  });
  await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
});

// Tests that entering search mode invalidates pageproxystate and that
// pageproxystate remains invalid after exiting search mode.
add_task(async function invalidate_pageproxystate() {
  await BrowserTestUtils.withNewTab("about:robots", async function(browser) {
    await UrlbarTestUtils.promisePopupOpen(window, () => {
      EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
    });
    Assert.equal(gURLBar.getAttribute("pageproxystate"), "valid");
    await UrlbarTestUtils.enterSearchMode(window);
    Assert.equal(
      gURLBar.getAttribute("pageproxystate"),
      "invalid",
      "Entering search mode should clear pageproxystate."
    );
    Assert.equal(gURLBar.value, "", "Urlbar value should be cleared.");
    await UrlbarTestUtils.exitSearchMode(window, { clickClose: true });
    Assert.equal(
      gURLBar.getAttribute("pageproxystate"),
      "invalid",
      "Pageproxystate should still be invalid after exiting search mode."
    );
  });
});
