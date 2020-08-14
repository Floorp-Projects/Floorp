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
    set: [
      ["browser.search.separatePrivateDefault.ui.enabled", false],
      ["browser.urlbar.update2", true],
      ["browser.urlbar.update2.localOneOffs", true],
      ["browser.urlbar.update2.oneOffsRefresh", true],
    ],
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

// Tests the indicator's interaction with the ESC key.
add_task(async function escape() {
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
  UrlbarTestUtils.assertSearchMode(window, {
    engineName: oneOffs[0].engine.name,
  });

  EventUtils.synthesizeKey("KEY_Escape");
  Assert.ok(!UrlbarTestUtils.isPopupOpen(window, "UrlbarView is closed."));
  Assert.ok(!gURLBar.value, "Urlbar value is empty.");
  UrlbarTestUtils.assertSearchMode(window, null);
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

// Tests that Accel+K enters search mode with the default engine.
add_task(async function keyboard_shortcut() {
  UrlbarTestUtils.assertSearchMode(window, null);
  EventUtils.synthesizeKey("k", { accelKey: true });
  UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    engineName: defaultEngine.name,
  });
  await UrlbarTestUtils.exitSearchMode(window, {
    clickClose: true,
    waitForSearch: false,
  });
});

// Tests that the Tools:Search menu item enters search mode with the default
// engine.
add_task(async function menubar_item() {
  UrlbarTestUtils.assertSearchMode(window, null);
  let command = window.document.getElementById("Tools:Search");
  command.doCommand();
  UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    engineName: defaultEngine.name,
  });
  await UrlbarTestUtils.exitSearchMode(window, {
    clickClose: true,
    waitForSearch: false,
  });
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

// Tests that the user doesn't get trapped in search mode if the update2 pref
// is disabled after entering search mode.
add_task(async function pref_flip_while_enabled() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_QUERY,
  });
  await UrlbarTestUtils.enterSearchMode(window);
  await verifySearchModeResultsAdded(window);
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.update2", false]],
  });
  UrlbarTestUtils.assertSearchMode(window, null);
  await SpecialPowers.popPrefEnv();
});

// Tests that search mode is stored per tab and restored when switching tabs.
add_task(async function tab_switch() {
  // Open three tabs.  We'll enter search mode in tabs 0 and 2.
  let tabs = [];
  for (let i = 0; i < 3; i++) {
    let tab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      url: "http://example.com/" + i,
    });
    tabs.push(tab);
  }

  // Switch to tab 0.
  await BrowserTestUtils.switchTab(gBrowser, tabs[0]);

  // Do a search and enter search mode.  Pass fireInputEvent so that
  // userTypedValue is set and restored when we switch back to this tab.  This
  // isn't really necessary but it simulates the user's typing, and it also
  // means that we'll start a search when we switch back to this tab.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_QUERY,
    fireInputEvent: true,
  });
  await UrlbarTestUtils.enterSearchMode(window);

  // Switch to tab 1.  Search mode should be exited.
  await BrowserTestUtils.switchTab(gBrowser, tabs[1]);
  UrlbarTestUtils.assertSearchMode(window, null);

  // Switch back to tab 0.  We should do a search (for TEST_QUERY) and re-enter
  // search mode.
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  await BrowserTestUtils.switchTab(gBrowser, tabs[0]);
  await searchPromise;
  let oneOffs = UrlbarTestUtils.getOneOffSearchButtons(
    window
  ).getSelectableButtons(true);
  UrlbarTestUtils.assertSearchMode(window, {
    engineName: oneOffs[0].engine.name,
  });

  // Switch to tab 2.  Search mode should be exited.
  await BrowserTestUtils.switchTab(gBrowser, tabs[2]);
  UrlbarTestUtils.assertSearchMode(window, null);

  // Do another search (in tab 2) and enter search mode.  This time, click a
  // local one-off just to test a different source.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_QUERY + " tab 2",
    fireInputEvent: true,
  });
  let localOneOff = UrlbarTestUtils.getOneOffSearchButtons(window)
    .localButtons[0];
  Assert.ok(
    localOneOff,
    "Sanity check: There should be at least one local one-off"
  );
  Assert.ok(
    localOneOff.source,
    "Sanity check: Local one-off should have a truthy source"
  );
  searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeMouseAtCenter(localOneOff, {});
  await searchPromise;
  Assert.ok(UrlbarTestUtils.isPopupOpen(window), "Urlbar view is still open.");
  UrlbarTestUtils.assertSearchMode(window, {
    source: localOneOff.source,
  });

  // Switch back to tab 0.  We should do a search and still be in search mode.
  searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  await BrowserTestUtils.switchTab(gBrowser, tabs[0]);
  await searchPromise;
  UrlbarTestUtils.assertSearchMode(window, {
    engineName: oneOffs[0].engine.name,
  });

  // Switch to tab 1.  Search mode should be exited.
  await BrowserTestUtils.switchTab(gBrowser, tabs[1]);
  UrlbarTestUtils.assertSearchMode(window, null);

  // Switch back to tab 2.  We should do a search and re-enter search mode.
  searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  await BrowserTestUtils.switchTab(gBrowser, tabs[2]);
  await searchPromise;
  UrlbarTestUtils.assertSearchMode(window, {
    source: localOneOff.source,
  });

  // Exit search mode.
  await UrlbarTestUtils.exitSearchMode(window, { clickClose: true });

  // Switch to tab 0.  We should do a search and re-enter search mode.
  searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  await BrowserTestUtils.switchTab(gBrowser, tabs[0]);
  await searchPromise;
  UrlbarTestUtils.assertSearchMode(window, {
    engineName: oneOffs[0].engine.name,
  });

  // Switch back to tab 2.  We should do a search but search mode should be
  // exited.
  searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  await BrowserTestUtils.switchTab(gBrowser, tabs[2]);
  await searchPromise;
  UrlbarTestUtils.assertSearchMode(window, null);

  // Switch back to tab 0.  We should do a search and re-enter search mode.
  searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  await BrowserTestUtils.switchTab(gBrowser, tabs[0]);
  await searchPromise;
  UrlbarTestUtils.assertSearchMode(window, {
    engineName: oneOffs[0].engine.name,
  });

  // Exit search mode.
  await UrlbarTestUtils.exitSearchMode(window, { clickClose: true });

  // Switch back to tab 2.  We should do a search but no search mode.
  searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  await BrowserTestUtils.switchTab(gBrowser, tabs[2]);
  await searchPromise;
  UrlbarTestUtils.assertSearchMode(window, null);

  // Switch back to tab 0.  We should do a search but no search mode.
  searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  await BrowserTestUtils.switchTab(gBrowser, tabs[0]);
  await searchPromise;
  UrlbarTestUtils.assertSearchMode(window, null);

  await UrlbarTestUtils.promisePopupClose(window);
  for (let tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }
});
