/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const SUGGEST_ALL_PREF = "browser.search.suggest.enabled";
const SUGGEST_URLBAR_PREF = "browser.urlbar.suggest.searches";
const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

add_task(async function switchToTab() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:about");

  await promiseAutocompleteResultPopup("% about");
  let result = await waitForAutocompleteResultAt(1);
  is(result.getAttribute("type"), "switchtab", "Expect right type attribute");
  is(result.label, "about:about about:about Tab", "Result a11y label should be: <title> <url> Tab");

  gURLBar.popup.hidePopup();
  await promisePopupHidden(gURLBar.popup);
  gBrowser.removeTab(tab);
});

add_task(async function searchSuggestions() {
  let engine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME);
  let oldCurrentEngine = Services.search.currentEngine;
  Services.search.currentEngine = engine;
  Services.prefs.setBoolPref(SUGGEST_ALL_PREF, true);
  let suggestionsEnabled = Services.prefs.getBoolPref(SUGGEST_URLBAR_PREF);
  Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, true);
  registerCleanupFunction(function() {
    Services.search.currentEngine = oldCurrentEngine;
    Services.prefs.clearUserPref(SUGGEST_ALL_PREF);
    Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, suggestionsEnabled);
  });

  await promiseAutocompleteResultPopup("foo");
  await waitForAutocompleteResultAt(2);
  // Don't assume that the search doesn't match history or bookmarks left around
  // by earlier tests.
  Assert.ok(gURLBar.popup.richlistbox.children.length >= 3,
            "Should get at least heuristic result + two search suggestions");
  // The first expected search is the search term itself since the heuristic
  // result will come before the search suggestions.
  let expectedSearches = [
    "foo",
    "foofoo",
    "foobar",
  ];
  for (let child of gURLBar.popup.richlistbox.children) {
    if (child.getAttribute("type").split(/\s+/).includes("searchengine")) {
      Assert.ok(expectedSearches.length > 0);
      let suggestion = expectedSearches.shift();
      Assert.equal(child.label, suggestion + " browser_searchSuggestionEngine searchSuggestionEngine.xml Search",
                   "Result label should be: <search term> <engine name> Search");
    }
  }
  Assert.ok(expectedSearches.length == 0);
  gURLBar.popup.hidePopup();
  await promisePopupHidden(gURLBar.popup);
});
