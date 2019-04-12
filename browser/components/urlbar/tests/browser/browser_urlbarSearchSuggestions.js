/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This tests checks that search suggestions can be acted upon correctly
 * e.g. selection with modifiers, copying text.
 */

"use strict";

const SUGGEST_URLBAR_PREF = "browser.urlbar.suggest.searches";
const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

// Must run first.
add_task(async function prepare() {
  let suggestionsEnabled = Services.prefs.getBoolPref(SUGGEST_URLBAR_PREF);
  Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, true);
  let engine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME);
  let oldDefaultEngine = await Services.search.getDefault();
  await Services.search.setDefault(engine);
  registerCleanupFunction(async function() {
    Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, suggestionsEnabled);
    await Services.search.setDefault(oldDefaultEngine);

    // Clicking suggestions causes visits to search results pages, so clear that
    // history now.
    await PlacesUtils.history.clear();
  });
});

add_task(async function clickSuggestion() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  gURLBar.focus();
  await promiseAutocompleteResultPopup("foo");
  let [idx, suggestion, engineName] = await promiseFirstSuggestion();
  Assert.equal(engineName,
               "browser_searchSuggestionEngine searchSuggestionEngine.xml",
               "Expected suggestion engine");

  let uri = (await Services.search.getDefault()).getSubmission(suggestion).uri;
  let loadPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser,
                                                   false, uri.spec);
  let element = await UrlbarTestUtils.waitForAutocompleteResultAt(window, idx);
  EventUtils.synthesizeMouseAtCenter(element, {}, window);
  await loadPromise;
  BrowserTestUtils.removeTab(tab);
});

async function testPressEnterOnSuggestion(expectedUrl = null, keyModifiers = {}) {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  gURLBar.focus();
  await promiseAutocompleteResultPopup("foo");
  let [idx, suggestion, engineName] = await promiseFirstSuggestion();
  Assert.equal(engineName,
               "browser_searchSuggestionEngine searchSuggestionEngine.xml",
               "Expected suggestion engine");

  if (!expectedUrl) {
    expectedUrl = (await Services.search.getDefault()).getSubmission(suggestion).uri.spec;
  }

  let promiseLoad =
    BrowserTestUtils.waitForDocLoadAndStopIt(expectedUrl, gBrowser.selectedBrowser);

  for (let i = 0; i < idx; ++i) {
    EventUtils.synthesizeKey("KEY_ArrowDown");
  }
  EventUtils.synthesizeKey("KEY_Enter", keyModifiers);

  await promiseLoad;
  BrowserTestUtils.removeTab(tab);
}

add_task(async function plainEnterOnSuggestion() {
  await testPressEnterOnSuggestion();
});

add_task(async function ctrlEnterOnSuggestion() {
  await testPressEnterOnSuggestion("http://www.foofoo.com/", { ctrlKey: true });
});

add_task(async function copySuggestionText() {
  gURLBar.focus();
  await promiseAutocompleteResultPopup("foo");
  let [idx, suggestion] = await promiseFirstSuggestion();
  for (let i = 0; i < idx; ++i) {
    EventUtils.synthesizeKey("KEY_ArrowDown");
  }
  gURLBar.select();
  await new Promise((resolve, reject) => waitForClipboard(suggestion, function() {
    goDoCommand("cmd_copy");
  }, resolve, reject));
});

async function getFirstSuggestion() {
  let matchCount = UrlbarTestUtils.getResultCount(window);
  for (let i = 0; i < matchCount; i++) {
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    if (result.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
        result.searchParams.suggestion) {
      return [i, result.searchParams.suggestion, result.searchParams.engine];
    }
  }
  return [-1, null, null];
}

async function promiseFirstSuggestion() {
  await UrlbarTestUtils.promiseSuggestionsPresent(window);
  return getFirstSuggestion();
}
