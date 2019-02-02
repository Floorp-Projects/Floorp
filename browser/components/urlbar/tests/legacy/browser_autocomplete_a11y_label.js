/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This test ensures that we produce good labels for a11y purposes.
 */

const SUGGEST_ALL_PREF = "browser.search.suggest.enabled";
const SUGGEST_URLBAR_PREF = "browser.urlbar.suggest.searches";
const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

add_task(async function switchToTab() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:about");

  await promiseAutocompleteResultPopup("% about");
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
    "Should have a switch tab result");

  // XXX Bug 1524539. This fails on QuantumBar because we're producing different
  // outputs. Once we confirm accessibilty is ok with the new format, we
  // should update and have this test running on QuantumBar.
  let element = await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1);
  is(element.label, "about:about about:about Tab", "Result a11y label should be: <title> <url> Tab");

  await UrlbarTestUtils.promisePopupClose(window);
  gBrowser.removeTab(tab);
});

add_task(async function searchSuggestions() {
  let engine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME);
  let oldDefaultEngine = await Services.search.getDefault();
  await Services.search.setDefault(engine);
  Services.prefs.setBoolPref(SUGGEST_ALL_PREF, true);
  let suggestionsEnabled = Services.prefs.getBoolPref(SUGGEST_URLBAR_PREF);
  Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, true);
  registerCleanupFunction(async function() {
    await Services.search.setDefault(oldDefaultEngine);
    Services.prefs.clearUserPref(SUGGEST_ALL_PREF);
    Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, suggestionsEnabled);
  });

  await promiseAutocompleteResultPopup("foo");
  let length = await UrlbarTestUtils.getResultCount(window);
  // Don't assume that the search doesn't match history or bookmarks left around
  // by earlier tests.
  Assert.greaterOrEqual(length, 3,
    "Should get at least heuristic result + two search suggestions");
  // The first expected search is the search term itself since the heuristic
  // result will come before the search suggestions.
  let expectedSearches = [
    "foo",
    "foofoo",
    "foobar",
  ];
  for (let i = 0; i < length; i++) {
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    if (result.type === UrlbarUtils.RESULT_TYPE.SEARCH) {
      Assert.greaterOrEqual(expectedSearches.length, 0,
        "Should still have expected searches remaining");
      let suggestion = expectedSearches.shift();
      // XXX Bug 1524539. This fails on QuantumBar because we're producing different
      // outputs. Once we confirm accessibilty is ok with the new format, we
      // should update and have this test running on QuantumBar.
      let element = await UrlbarTestUtils.waitForAutocompleteResultAt(window, i);
      Assert.equal(element.label,
        suggestion + " browser_searchSuggestionEngine searchSuggestionEngine.xml Search",
        "Result label should be: <search term> <engine name> Search");
    }
  }
  Assert.ok(expectedSearches.length == 0);
  await UrlbarTestUtils.promisePopupClose(window);
});
