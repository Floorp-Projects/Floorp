/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This file tests the urlbar.searchmode.* scalars telemetry with search mode
 * related actions.
 */

"use strict";

const ENTRY_SCALAR_PREFIX = "urlbar.searchmode.";
const PICKED_SCALAR_PREFIX = "urlbar.picked.searchmode.";
const ENGINE_ALIAS = "alias";
const TEST_QUERY = "test";
let engineName;
let engineDomain;

// The preference to enable suggestions.
const SUGGEST_PREF = "browser.search.suggest.enabled";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarProviderTabToSearch:
    "resource:///modules/UrlbarProviderTabToSearch.sys.mjs",
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "TouchBarHelper",
  "@mozilla.org/widget/touchbarhelper;1",
  "nsITouchBarHelper"
);

/**
 * Asserts that search mode telemetry was recorded correctly. Checks both the
 * urlbar.searchmode.* and urlbar.searchmode_picked.* probes.
 * @param {string} entry
 *   A search mode entry point.
 * @param {string} engineOrSource
 *   An engine name or a search mode source.
 * @param {number} [resultIndex]
 *   The index of the result picked while in search mode. Only pass this
 *   parameter if a result is picked.
 */
function assertSearchModeScalars(entry, engineOrSource, resultIndex = -1) {
  // Check if the urlbar.searchmode.entry scalar contains the expected value.
  const scalars = TelemetryTestUtils.getProcessScalars("parent", true, false);
  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    ENTRY_SCALAR_PREFIX + entry,
    engineOrSource,
    1
  );

  for (let e of UrlbarUtils.SEARCH_MODE_ENTRY) {
    if (e == entry) {
      Assert.equal(
        Object.keys(scalars[ENTRY_SCALAR_PREFIX + entry]).length,
        1,
        `This search must only increment one entry in the correct scalar: ${e}`
      );
    } else {
      Assert.ok(
        !scalars[ENTRY_SCALAR_PREFIX + e],
        `No other urlbar.searchmode scalars should be recorded. Checking ${e}`
      );
    }
  }

  if (resultIndex >= 0) {
    TelemetryTestUtils.assertKeyedScalar(
      scalars,
      PICKED_SCALAR_PREFIX + entry,
      resultIndex,
      1
    );
  }

  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();
}

add_setup(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Disable tab-to-search onboarding results for general tests. They are
      // enabled in tests that specifically address onboarding.
      ["browser.urlbar.tabToSearch.onboard.interactionsLeft", 0],
    ],
  });

  // Create an engine to generate search suggestions and add it as default
  // for this test.
  let suggestionEngine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + "urlbarTelemetrySearchSuggestions.xml"
  );
  suggestionEngine.alias = ENGINE_ALIAS;
  engineDomain = suggestionEngine.getResultDomain();
  engineName = suggestionEngine.name;

  // Make it the default search engine.
  let originalEngine = await Services.search.getDefault();
  await Services.search.setDefault(suggestionEngine);

  // And the first one-off engine.
  await Services.search.moveEngine(suggestionEngine, 0);

  // Enable local telemetry recording for the duration of the tests.
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;

  // Clear history so that history added by previous tests doesn't mess up this
  // test when it selects results in the urlbar.
  await PlacesUtils.history.clear();

  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();

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
    await PlacesUtils.history.clear();
    Services.telemetry.setEventRecordingEnabled("navigation", false);
    UrlbarTestUtils.uninit();
  });
});

// Clicks the first one off.
add_task(async function test_oneoff_remote() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_QUERY,
  });
  // Enters search mode by clicking a one-off.
  await UrlbarTestUtils.enterSearchMode(window);
  let loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  EventUtils.synthesizeKey("KEY_Enter");
  await loadPromise;
  assertSearchModeScalars("oneoff", "other", 0);

  BrowserTestUtils.removeTab(tab);
});

// Clicks the history one off.
add_task(async function test_oneoff_local() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_QUERY,
  });
  // Enters search mode by clicking a one-off.
  await UrlbarTestUtils.enterSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.HISTORY,
  });
  let loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  EventUtils.synthesizeKey("KEY_ArrowDown");
  EventUtils.synthesizeKey("KEY_Enter");
  await loadPromise;
  assertSearchModeScalars("oneoff", "history", 0);

  BrowserTestUtils.removeTab(tab);
});

// Checks that the Amazon search mode name is collapsed to "Amazon".
add_task(async function test_oneoff_amazon() {
  // Disable suggestions to avoid hitting Amazon servers.
  await SpecialPowers.pushPrefEnv({
    set: [[SUGGEST_PREF, false]],
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_QUERY,
  });
  // Enters search mode by clicking a one-off.
  await UrlbarTestUtils.enterSearchMode(window, {
    engineName: "Amazon.com",
  });
  assertSearchModeScalars("oneoff", "Amazon");
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

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_QUERY,
  });
  // Enters search mode by clicking a one-off.
  await UrlbarTestUtils.enterSearchMode(window, {
    engineName: "Wikipedia (en)",
  });
  assertSearchModeScalars("oneoff", "Wikipedia");
  await UrlbarTestUtils.exitSearchMode(window);

  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});

// Enters search mode by pressing the keyboard shortcut.
add_task(async function test_shortcut() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_QUERY,
  });
  // Enter search mode by pressing the keyboard shortcut.
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeKey("k", { accelKey: true });
  await searchPromise;

  await UrlbarTestUtils.assertSearchMode(window, {
    engineName,
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    entry: "shortcut",
  });
  assertSearchModeScalars("shortcut", "other");

  BrowserTestUtils.removeTab(tab);
});

// Enters search mode by selecting a Top Site from the Urlbar.
add_task(async function test_topsites_urlbar() {
  // Disable suggestions to avoid hitting Amazon servers.
  await SpecialPowers.pushPrefEnv({
    set: [[SUGGEST_PREF, false]],
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

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
  assertSearchModeScalars("topsites_urlbar", "Amazon");
  await UrlbarTestUtils.exitSearchMode(window);

  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});

// Enters search mode by selecting a keyword offer result.
add_task(async function test_keywordoffer() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

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

  // Pick the keyword offer result.
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeKey("KEY_Enter");
  await searchPromise;

  await UrlbarTestUtils.assertSearchMode(window, {
    engineName,
    entry: "keywordoffer",
  });
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "foo",
  });
  let loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  EventUtils.synthesizeKey("KEY_Enter");
  await loadPromise;
  assertSearchModeScalars("keywordoffer", "other", 0);

  BrowserTestUtils.removeTab(tab);
});

// Enters search mode by typing an alias.
add_task(async function test_typed() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  // Enter search mode by selecting a keywordoffer result.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: `${ENGINE_ALIAS} `,
  });

  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeKey(" ");
  await searchPromise;

  await UrlbarTestUtils.assertSearchMode(window, {
    engineName,
    entry: "typed",
  });
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "foo",
  });
  let loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  EventUtils.synthesizeKey("KEY_Enter");
  await loadPromise;
  assertSearchModeScalars("typed", "other", 0);

  BrowserTestUtils.removeTab(tab);
});

// Enters search mode by calling the same function called by the Search
// Bookmarks menu item in Library > Bookmarks.
add_task(async function test_bookmarkmenu() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  PlacesCommandHook.searchBookmarks();
  await searchPromise;

  await UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    entry: "bookmarkmenu",
  });
  assertSearchModeScalars("bookmarkmenu", "bookmarks");
  BrowserTestUtils.removeTab(tab);
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
  assertSearchModeScalars("tabmenu", "tabs");
});

// Enters search mode by performing a search handoff on about:privatebrowsing.
// Note that handoff-to-search-mode only occurs when suggestions are disabled
// in the Urlbar.
// NOTE: We don't test handoff on about:home. Running mochitests on about:home
// is quite difficult. This subtest verifies that `handoff` is a valid scalar
// suffix and that a call to UrlbarInput.handoff(value, searchEngine) records
// values in the urlbar.searchmode.handoff scalar. PlacesFeed.test.js verfies that
// about:home handoff makes that exact call.
add_task(async function test_handoff_pbm() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.searches", false]],
  });
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
    engineName,
    entry: "handoff",
  });
  assertSearchModeScalars("handoff", "other");

  await UrlbarTestUtils.exitSearchMode(win);
  await UrlbarTestUtils.promisePopupClose(win);
  await BrowserTestUtils.closeWindow(win);
  await SpecialPowers.popPrefEnv();
});

// Enters search mode by tapping a search shortcut on the Touch Bar.
add_task(async function test_touchbar() {
  if (AppConstants.platform != "macosx") {
    return;
  }

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_QUERY,
  });
  // We have to fake the tap on the Touch Bar since mochitests have no way of
  // interacting with the Touch Bar.
  TouchBarHelper.insertRestrictionInUrlbar(UrlbarTokenizer.RESTRICT.HISTORY);
  await UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.HISTORY,
    entry: "touchbar",
  });
  let loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  EventUtils.synthesizeKey("KEY_ArrowDown");
  EventUtils.synthesizeKey("KEY_Enter");
  await loadPromise;
  assertSearchModeScalars("touchbar", "history", 0);
  BrowserTestUtils.removeTab(tab);
});

// Enters search mode by selecting a tab-to-search result.
// Tests that tab-to-search results preview search mode when highlighted. These
// results are worth testing separately since they do not set the
// payload.keyword parameter.
add_task(async function test_tabtosearch() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Do not show the onboarding result for this subtest.
      ["browser.urlbar.tabToSearch.onboard.interactionsLeft", 0],
    ],
  });
  await PlacesTestUtils.addVisits([`http://${engineDomain}/`]);

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: engineDomain.slice(0, 4),
  });
  let tabToSearchResult = (
    await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1)
  ).result;
  Assert.equal(
    tabToSearchResult.providerName,
    "TabToSearch",
    "The second result is a tab-to-search result."
  );
  Assert.equal(
    tabToSearchResult.payload.engine,
    engineName,
    "The tab-to-search result is for the correct engine."
  );
  await UrlbarTestUtils.assertSearchMode(window, null);
  EventUtils.synthesizeKey("KEY_ArrowDown");
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(window),
    1,
    "Sanity check: The second result is selected."
  );
  // Pick the tab-to-search result.
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeKey("KEY_Enter");
  await searchPromise;

  await UrlbarTestUtils.assertSearchMode(window, {
    engineName,
    entry: "tabtosearch",
  });
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "foo",
  });
  let loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  EventUtils.synthesizeKey("KEY_Enter");
  await loadPromise;
  assertSearchModeScalars("tabtosearch", "other", 0);

  BrowserTestUtils.removeTab(tab);

  await PlacesUtils.history.clear();
  await SpecialPowers.popPrefEnv();
});

// Enters search mode by selecting a tab-to-search onboarding result.
add_task(async function test_tabtosearch_onboard() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.tabToSearch.onboard.interactionsLeft", 3]],
  });
  await PlacesTestUtils.addVisits([`http://${engineDomain}/`]);

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: engineDomain.slice(0, 4),
    fireInputEvent: true,
  });
  let tabToSearchResult = (
    await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1)
  ).result;
  Assert.equal(
    tabToSearchResult.providerName,
    "TabToSearch",
    "The second result is a tab-to-search result."
  );
  Assert.equal(
    tabToSearchResult.payload.engine,
    engineName,
    "The tab-to-search result is for the correct engine."
  );
  Assert.equal(
    tabToSearchResult.payload.dynamicType,
    "onboardTabToSearch",
    "The tab-to-search result is an onboarding result."
  );
  await UrlbarTestUtils.assertSearchMode(window, null);
  EventUtils.synthesizeKey("KEY_ArrowDown");
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(window),
    1,
    "Sanity check: The second result is selected."
  );
  // Pick the tab-to-search onboarding result.
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeKey("KEY_Enter");
  await searchPromise;

  await UrlbarTestUtils.assertSearchMode(window, {
    engineName,
    entry: "tabtosearch_onboard",
  });

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "foo",
  });
  let loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  EventUtils.synthesizeKey("KEY_Enter");
  await loadPromise;
  assertSearchModeScalars("tabtosearch_onboard", "other", 0);

  UrlbarPrefs.set("tabToSearch.onboard.interactionsLeft", 3);
  delete UrlbarProviderTabToSearch.onboardingInteractionAtTime;

  BrowserTestUtils.removeTab(tab);
  await PlacesUtils.history.clear();
  await SpecialPowers.popPrefEnv();
});
