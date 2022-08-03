/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that one-offs behave differently with key modifiers.
 */

"use strict";

const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";
const SEARCH_STRING = "foo.bar";

XPCOMUtils.defineLazyGetter(this, "oneOffSearchButtons", () => {
  return UrlbarTestUtils.getOneOffSearchButtons(window);
});

let engine;

async function searchAndOpenPopup(value) {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value,
    fireInputEvent: true,
  });
  await TestUtils.waitForCondition(
    () => !oneOffSearchButtons._rebuilding,
    "Waiting for one-offs to finish rebuilding"
  );
}

add_task(async function init() {
  // Add a search suggestion engine and move it to the front so that it appears
  // as the first one-off.
  engine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME
  );
  await Services.search.moveEngine(engine, 0);

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.separatePrivateDefault.ui.enabled", false],
      ["browser.urlbar.suggest.quickactions", false],
    ],
  });

  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
    await UrlbarTestUtils.formHistory.clear();
  });

  // Initialize history with enough visits to fill up the view.
  await PlacesUtils.history.clear();
  await UrlbarTestUtils.formHistory.clear();
  let maxResults = Services.prefs.getIntPref("browser.urlbar.maxRichResults");
  for (let i = 0; i < maxResults; i++) {
    await PlacesTestUtils.addVisits(
      "http://mochi.test:8888/browser_urlbarOneOffs.js/?" + i
    );
  }

  // Add some more visits to the last URL added above so that the top-sites view
  // will be non-empty.
  for (let i = 0; i < 5; i++) {
    await PlacesTestUtils.addVisits(
      "http://mochi.test:8888/browser_urlbarOneOffs.js/?" + (maxResults - 1)
    );
  }
  await updateTopSites(sites => {
    return (
      sites && sites[0] && sites[0].url.startsWith("http://mochi.test:8888/")
    );
  });

  // Move the mouse away from the view so that a result or one-off isn't
  // inadvertently highlighted.  See bug 1659011.
  EventUtils.synthesizeMouse(
    gURLBar.inputField,
    0,
    0,
    { type: "mousemove" },
    window
  );
});

// Shift clicking with no search string should open search mode, like an
// unmodified click.
add_task(async function shift_click_empty() {
  await searchAndOpenPopup("");
  let oneOffs = oneOffSearchButtons.getSelectableButtons(true);
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeMouseAtCenter(oneOffs[0], { shiftKey: true });
  await searchPromise;
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: oneOffs[0].engine.name,
    entry: "oneoff",
  });
  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window);
});

// Shift clicking with a search string should perform a search in the current
// tab.
add_task(async function shift_click_search() {
  await searchAndOpenPopup(SEARCH_STRING);
  let resultsPromise = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    "http://mochi.test:8888/?terms=foo.bar"
  );
  let oneOffs = oneOffSearchButtons.getSelectableButtons(true);
  EventUtils.synthesizeMouseAtCenter(oneOffs[0], { shiftKey: true });
  await resultsPromise;
  await UrlbarTestUtils.assertSearchMode(window, null);
  await UrlbarTestUtils.promisePopupClose(window);
});

// Pressing Shift+Enter on a one-off with no search string should open search
// mode, like an unmodified click.
add_task(async function shift_enter_empty() {
  await searchAndOpenPopup("");
  // Alt+Down to select the first one-off.
  let oneOffs = oneOffSearchButtons.getSelectableButtons(true);
  EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeKey("KEY_Enter", { shiftKey: true });
  await searchPromise;
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: oneOffs[0].engine.name,
    entry: "oneoff",
  });
  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window);
});

// Pressing Shift+Enter on a one-off with a search string should perform a
// search in the current tab.
add_task(async function shift_enter_search() {
  await searchAndOpenPopup(SEARCH_STRING);
  let resultsPromise = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    "http://mochi.test:8888/?terms=foo.bar"
  );
  EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
  EventUtils.synthesizeKey("KEY_Enter", { shiftKey: true });
  await resultsPromise;
  await UrlbarTestUtils.assertSearchMode(window, null);
  await UrlbarTestUtils.promisePopupClose(window);
});

// Pressing Alt+Enter on a one-off on an "empty" page (e.g. new tab) should open
// search mode in the current tab.
add_task(async function alt_enter_emptypage() {
  await BrowserTestUtils.withNewTab("about:home", async function(browser) {
    await searchAndOpenPopup(SEARCH_STRING);
    let oneOffs = oneOffSearchButtons.getSelectableButtons(true);
    // Alt+Down to select the first one-off.
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
    let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
    EventUtils.synthesizeKey("KEY_Enter", { altKey: true });
    await searchPromise;
    Assert.equal(
      browser,
      gBrowser.selectedBrowser,
      "The foreground tab hasn't changed."
    );
    await UrlbarTestUtils.assertSearchMode(window, {
      engineName: oneOffs[0].engine.name,
      entry: "oneoff",
    });
    await UrlbarTestUtils.exitSearchMode(window);
    await UrlbarTestUtils.promisePopupClose(window);
  });
});

// Pressing Alt+Enter on a one-off with no search string and on a "non-empty"
// page should open search mode in a new foreground tab.
add_task(async function alt_enter_empty() {
  await BrowserTestUtils.withNewTab("about:robots", async function(browser) {
    await searchAndOpenPopup("");
    let oneOffs = oneOffSearchButtons.getSelectableButtons(true);
    // Alt+Down to select the first one-off.
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
    let tabOpenPromise = BrowserTestUtils.waitForEvent(
      gBrowser.tabContainer,
      "TabOpen"
    );
    EventUtils.synthesizeKey("KEY_Enter", { altKey: true });
    await tabOpenPromise;
    Assert.notEqual(
      browser,
      gBrowser.selectedBrowser,
      "The current foreground tab is new."
    );
    await UrlbarTestUtils.assertSearchMode(window, {
      engineName: oneOffs[0].engine.name,
      entry: "oneoff",
    });
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
    Assert.equal(
      browser,
      gBrowser.selectedBrowser,
      "We're back in the original tab."
    );
    await UrlbarTestUtils.assertSearchMode(window, null);
    await UrlbarTestUtils.promisePopupClose(window);
  });
});

// Pressing Alt+Enter on a remote one-off with a search string and on a
// "non-empty" page should perform a search in a new foreground tab.
add_task(async function alt_enter_search_remote() {
  await BrowserTestUtils.withNewTab("about:robots", async function(browser) {
    await searchAndOpenPopup(SEARCH_STRING);
    // Alt+Down to select the first one-off.
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
    let tabOpenPromise = BrowserTestUtils.waitForNewTab(
      gBrowser,
      "http://mochi.test:8888/?terms=foo.bar",
      true
    );
    EventUtils.synthesizeKey("KEY_Enter", { altKey: true });
    // This implictly checks the correct page is loaded.
    let newTab = await tabOpenPromise;
    Assert.equal(
      newTab,
      gBrowser.selectedTab,
      "The current foreground tab is new."
    );
    // Check search mode is not activated in the new tab.
    await UrlbarTestUtils.assertSearchMode(window, null);
    BrowserTestUtils.removeTab(newTab);
    Assert.equal(
      browser,
      gBrowser.selectedBrowser,
      "We're back in the original tab."
    );
    await UrlbarTestUtils.assertSearchMode(window, null);
    await UrlbarTestUtils.promisePopupClose(window);
  });
});

// Pressing Alt+Enter on a local one-off with a search string and on a
// "non-empty" page should open search mode in a new foreground tab with the
// search string already populated.
add_task(async function alt_enter_search_local() {
  await BrowserTestUtils.withNewTab("about:robots", async function(browser) {
    await searchAndOpenPopup(SEARCH_STRING);
    // Alt+Down to select the first local one-off.
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
    while (
      oneOffSearchButtons.selectedButton.id !=
      "urlbar-engine-one-off-item-bookmarks"
    ) {
      EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
    }
    let tabOpenPromise = BrowserTestUtils.waitForEvent(
      gBrowser.tabContainer,
      "TabOpen"
    );
    EventUtils.synthesizeKey("KEY_Enter", { altKey: true });
    await tabOpenPromise;
    Assert.notEqual(
      browser,
      gBrowser.selectedBrowser,
      "The current foreground tab is new."
    );
    await UrlbarTestUtils.assertSearchMode(window, {
      source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
      entry: "oneoff",
    });
    Assert.equal(
      gURLBar.value,
      SEARCH_STRING,
      "The search term was duplicated to the new tab."
    );
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
    Assert.equal(
      browser,
      gBrowser.selectedBrowser,
      "We're back in the original tab."
    );
    await UrlbarTestUtils.assertSearchMode(window, null);
    await UrlbarTestUtils.promisePopupClose(window);
  });
});

// Accel+Clicking a one-off with an empty search string should open search mode
// in a new background tab.
add_task(async function accel_click_empty() {
  await searchAndOpenPopup("");
  let oneOffs = oneOffSearchButtons.getSelectableButtons(true);

  // We have to listen for the new tab using this brute force method.
  // about:newtab is preloaded in the background. When about:newtab is opened,
  // the cached version is shown. Since the page is already loaded,
  // waitForNewTab does not detect it. It also doesn't fire the TabOpen event.
  let tabCount = gBrowser.tabs.length;
  let tabOpenPromise = TestUtils.waitForCondition(
    () =>
      gBrowser.tabs.length == tabCount + 1
        ? gBrowser.tabs[gBrowser.tabs.length - 1]
        : false,
    "Waiting for background about:newtab to open."
  );
  EventUtils.synthesizeMouseAtCenter(oneOffs[0], { accelKey: true });
  let newTab = await tabOpenPromise;
  Assert.notEqual(
    newTab.linkedBrowser,
    gBrowser.selectedBrowser,
    "The foreground tab hasn't changed."
  );
  await UrlbarTestUtils.assertSearchMode(window, null);
  BrowserTestUtils.switchTab(gBrowser, newTab);
  // Check the new background tab is already in search mode.
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: oneOffs[0].engine.name,
    entry: "oneoff",
  });
  BrowserTestUtils.removeTab(newTab);
  await UrlbarTestUtils.promisePopupClose(window);
});

// Accel+Clicking a remote one-off with a search string should execute a search
// in a new background tab.
add_task(async function accel_click_search_remote() {
  await searchAndOpenPopup(SEARCH_STRING);
  let oneOffs = oneOffSearchButtons.getSelectableButtons(true);
  let tabOpenPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "http://mochi.test:8888/?terms=foo.bar",
    true
  );
  EventUtils.synthesizeMouseAtCenter(oneOffs[0], { accelKey: true });
  // This implictly checks the correct page is loaded.
  let newTab = await tabOpenPromise;
  Assert.notEqual(
    gBrowser.selectedTab,
    newTab,
    "The foreground tab hasn't changed."
  );
  await UrlbarTestUtils.assertSearchMode(window, null);
  // Switch to the background tab, which is the last tab in gBrowser.tabs.
  BrowserTestUtils.switchTab(gBrowser, newTab);
  // Check the new background tab is not search mode.
  await UrlbarTestUtils.assertSearchMode(window, null);
  BrowserTestUtils.removeTab(newTab);
  await UrlbarTestUtils.promisePopupClose(window);
});

// Accel+Clicking a local one-off with a search string should open search mode
// in a new background tab with the search string already populated.
add_task(async function accel_click_search_local() {
  await searchAndOpenPopup(SEARCH_STRING);
  let oneOffs = oneOffSearchButtons.getSelectableButtons(true);
  let oneOff;
  for (oneOff of oneOffs) {
    if (oneOff.id == "urlbar-engine-one-off-item-bookmarks") {
      break;
    }
  }
  let tabCount = gBrowser.tabs.length;
  let tabOpenPromise = TestUtils.waitForCondition(
    () =>
      gBrowser.tabs.length == tabCount + 1
        ? gBrowser.tabs[gBrowser.tabs.length - 1]
        : false,
    "Waiting for background about:newtab to open."
  );
  EventUtils.synthesizeMouseAtCenter(oneOff, { accelKey: true });
  let newTab = await tabOpenPromise;
  Assert.notEqual(
    newTab.linkedBrowser,
    gBrowser.selectedBrowser,
    "The foreground tab hasn't changed."
  );
  await UrlbarTestUtils.assertSearchMode(window, null);
  BrowserTestUtils.switchTab(gBrowser, newTab);
  // Check the new background tab is already in search mode.
  await UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    entry: "oneoff",
  });
  // Check the search string is already populated.
  Assert.equal(
    gURLBar.value,
    SEARCH_STRING,
    "The search term was duplicated to the new tab."
  );
  BrowserTestUtils.removeTab(newTab);
  await UrlbarTestUtils.promisePopupClose(window);
});
