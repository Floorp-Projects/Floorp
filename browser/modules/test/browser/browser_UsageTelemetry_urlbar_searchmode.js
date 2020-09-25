/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This file tests the urlbar.searchmode.* scalars telemetry with search mode
 * related actions.
 */

"use strict";

const SCALAR_PREFIX = "urlbar.searchmode.";
const ENGINE_NAME = "MozSearch";
const ENGINE_ALIAS = "alias";
const TEST_QUERY = "test";

// The preference to enable suggestions.
const SUGGEST_PREF = "browser.search.suggest.enabled";

XPCOMUtils.defineLazyModuleGetters(this, {
  SearchTelemetry: "resource:///modules/SearchTelemetry.jsm",
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.jsm",
});

/**
 * Asserts that search mode telemetry was recorded correctly.
 * @param {string} entry
 * @param {string} key
 */
function assertSearchModeScalar(entry, key) {
  // Check if the urlbar.searchmode.entry scalar contains the expected value.
  const scalars = TelemetryTestUtils.getProcessScalars("parent", true, false);
  TelemetryTestUtils.assertKeyedScalar(scalars, SCALAR_PREFIX + entry, key, 1);

  for (let e of UrlbarUtils.SEARCH_MODE_ENTRY) {
    if (e == entry) {
      Assert.equal(
        Object.keys(scalars[SCALAR_PREFIX + entry]).length,
        1,
        `This search must only increment one entry in the correct scalar: ${e}`
      );
    } else {
      Assert.ok(
        !scalars[SCALAR_PREFIX + e],
        `No other urlbar.searchmode scalars should be recorded. Checking ${e}`
      );
    }
  }

  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.update2", true],
      ["browser.urlbar.update2.localOneOffs", true],
      ["browser.urlbar.update2.oneOffsRefresh", true],
    ],
  });

  // Create a new search engine.
  await Services.search.addEngineWithDetails(ENGINE_NAME, {
    alias: ENGINE_ALIAS,
    method: "GET",
    template: "http://example.com/?q={searchTerms}",
  });

  // Make it the default search engine.
  let engine = Services.search.getEngineByName(ENGINE_NAME);
  let originalEngine = await Services.search.getDefault();
  await Services.search.setDefault(engine);

  // And the first one-off engine.
  await Services.search.moveEngine(engine, 0);

  // Enable local telemetry recording for the duration of the tests.
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;

  // Clear history so that history added by previous tests doesn't mess up this
  // test when it selects results in the urlbar.
  await PlacesUtils.history.clear();

  // Clear historical search suggestions to avoid interference from previous
  // tests.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.maxHistoricalSearchSuggestions", 0]],
  });

  // Allows UrlbarTestUtils to access this scope's test helpers, like Assert.
  UrlbarTestUtils.init(this);

  // Make sure to restore the engine once we're done.
  registerCleanupFunction(async function() {
    Services.telemetry.canRecordExtended = oldCanRecord;
    await Services.search.setDefault(originalEngine);
    await Services.search.removeEngine(engine);
    await PlacesUtils.history.clear();
    Services.telemetry.setEventRecordingEnabled("navigation", false);
    UrlbarTestUtils.uninit();
  });
});

// Clicks the first one off.
add_task(async function test_oneoff_remote() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_QUERY,
  });
  // Enters search mode by clicking a one-off.
  await UrlbarTestUtils.enterSearchMode(window);
  assertSearchModeScalar("oneoff", "other");
  await UrlbarTestUtils.exitSearchMode(window);

  BrowserTestUtils.removeTab(tab);
});

// Clicks the history one off.
add_task(async function test_oneoff_local() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_QUERY,
  });
  // Enters search mode by clicking a one-off.
  await UrlbarTestUtils.enterSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.HISTORY,
  });
  assertSearchModeScalar("oneoff", "history");
  await UrlbarTestUtils.exitSearchMode(window);

  BrowserTestUtils.removeTab(tab);
});

// Checks that the Amazon search mode name is collapsed to "Amazon".
add_task(async function test_oneoff_amazon() {
  // Disable suggestions to avoid hitting Amazon servers.
  await SpecialPowers.pushPrefEnv({
    set: [[SUGGEST_PREF, false]],
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_QUERY,
  });
  // Enters search mode by clicking a one-off.
  await UrlbarTestUtils.enterSearchMode(window, {
    engineName: "Amazon.com",
  });
  assertSearchModeScalar("oneoff", "Amazon");
  await UrlbarTestUtils.exitSearchMode(window);

  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});

// Checks that the Wikipedia search mode name is collapsed to "Wikipedia".
add_task(async function test_oneoff_wikipedia() {
  // Disable suggestions to avoid hitting Wikipedia servers.
  await SpecialPowers.pushPrefEnv({
    set: [[SUGGEST_PREF, false]],
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_QUERY,
  });
  // Enters search mode by clicking a one-off.
  await UrlbarTestUtils.enterSearchMode(window, {
    engineName: "Wikipedia (en)",
  });
  assertSearchModeScalar("oneoff", "Wikipedia");
  await UrlbarTestUtils.exitSearchMode(window);

  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});

// Enters search mode by pressing the keyboard shortcut.
add_task(async function test_shortcut() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_QUERY,
  });
  // Enter search mode by pressing the keyboard shortcut.
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeKey("k", { accelKey: true });
  await searchPromise;

  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: ENGINE_NAME,
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    entry: "shortcut",
  });
  assertSearchModeScalar("shortcut", "other");
  await UrlbarTestUtils.exitSearchMode(window);

  BrowserTestUtils.removeTab(tab);
});

// Enters search mode by selecting a Top Site from the Urlbar.
add_task(async function test_topsites_urlbar() {
  // Disable suggestions to avoid hitting Wikipedia servers.
  await SpecialPowers.pushPrefEnv({
    set: [[SUGGEST_PREF, false]],
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  // Enter search mode by selecting a Top Site from the Urlbar.
  await UrlbarTestUtils.promisePopupOpen(window, () => {
    if (gURLBar.getAttribute("pageproxystate") == "invalid") {
      gURLBar.handleRevert();
    }
    EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
  });
  await UrlbarTestUtils.promiseSearchComplete(window);

  let amazonSearch = await UrlbarTestUtils.waitForAutocompleteResultAt(
    window,
    0
  );
  Assert.equal(
    amazonSearch.result.payload.keyword,
    "@amazon",
    "First result should have the Amazon keyword."
  );
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeMouseAtCenter(amazonSearch, {});
  await searchPromise;

  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: amazonSearch.result.payload.engine,
    entry: "topsites_urlbar",
  });
  assertSearchModeScalar("topsites_urlbar", "Amazon");
  await UrlbarTestUtils.exitSearchMode(window);

  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});

// Enters search mode by selecting a keyword offer result.
add_task(async function test_keywordoffer() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  // Do a search for "@" + our test alias.  It should autofill with a trailing
  // space, and the heuristic result should be an autofill result with a keyword
  // offer.
  let alias = "@" + ENGINE_ALIAS;
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: alias,
  });
  let keywordOfferResult = await UrlbarTestUtils.getDetailsOfResultAt(
    window,
    0
  );
  Assert.equal(
    keywordOfferResult.searchParams.keyword,
    alias,
    "The first result should be a keyword search result with the correct alias."
  );

  // Select the keyword offer.
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeKey("KEY_Enter");
  await searchPromise;

  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: ENGINE_NAME,
    entry: "keywordoffer",
  });
  assertSearchModeScalar("keywordoffer", "other");
  await UrlbarTestUtils.exitSearchMode(window);

  BrowserTestUtils.removeTab(tab);
});

// Enters search mode by typing an alias.
add_task(async function test_typed() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  // Enter search mode by selecting a keywordoffer result.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: `${ENGINE_ALIAS} `,
  });

  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeKey(" ");
  await searchPromise;

  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: ENGINE_NAME,
    entry: "typed",
  });
  assertSearchModeScalar("typed", "other");
  await UrlbarTestUtils.exitSearchMode(window);

  BrowserTestUtils.removeTab(tab);
});

// Enters search mode by calling the same function called by the Search
// Bookmarks menu item in Library > Bookmarks.
add_task(async function test_bookmarkmenu() {
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  PlacesCommandHook.searchBookmarks();
  await searchPromise;

  await UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    entry: "bookmarkmenu",
  });
  assertSearchModeScalar("bookmarkmenu", "bookmarks");
  await UrlbarTestUtils.exitSearchMode(window);
});

// Enters search mode by calling the same function called by the Search Tabs
// menu item in the tab overflow menu.
add_task(async function test_tabmenu() {
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  gTabsPanel.searchTabs();
  await searchPromise;

  await UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.TABS,
    entry: "tabmenu",
  });
  assertSearchModeScalar("tabmenu", "tabs");
  await UrlbarTestUtils.exitSearchMode(window);
});

// Enters search mode by performing a search handoff on about:privatebrowsing.
// NOTE: We don't test handoff on about:home. Running mochitests on about:home
// is quite difficult. This subtest verifies that `handoff` is a valid scalar
// suffix and that a call to
// UrlbarInput.search(value, { searchModeEntry: "handoff" }) records values in
// the urlbar.searchmode.handoff scalar. PlacesFeed.test.js verfies that
// about:home handoff makes that exact call.
add_task(async function test_handoff_pbm() {
  let win = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
    waitForTabURL: "about:privatebrowsing",
  });
  let tab = win.gBrowser.selectedBrowser;

  await SpecialPowers.spawn(tab, [], async function() {
    let btn = content.document.getElementById("search-handoff-button");
    btn.click();
  });

  let searchPromise = UrlbarTestUtils.promiseSearchComplete(win);
  await new Promise(r => EventUtils.synthesizeKey("f", {}, win, r));
  await searchPromise;
  await UrlbarTestUtils.assertSearchMode(win, {
    engineName: ENGINE_NAME,
    entry: "handoff",
  });
  assertSearchModeScalar("handoff", "other");

  await UrlbarTestUtils.exitSearchMode(win);
  await UrlbarTestUtils.promisePopupClose(win);
  await BrowserTestUtils.closeWindow(win);
});
