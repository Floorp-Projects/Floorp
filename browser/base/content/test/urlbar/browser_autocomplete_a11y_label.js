/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const SUGGEST_ALL_PREF = "browser.search.suggest.enabled";
const SUGGEST_URLBAR_PREF = "browser.urlbar.suggest.searches";
const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

add_task(function* switchToTab() {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:about");

  yield promiseAutocompleteResultPopup("% about");

  ok(gURLBar.popup.richlistbox.children.length > 1, "Should get at least 2 results");
  let result = gURLBar.popup.richlistbox.children[1];
  is(result.getAttribute("type"), "switchtab", "Expect right type attribute");
  is(result.label, "about:about about:about Tab", "Result a11y label should be: <title> <url> Tab");

  gURLBar.popup.hidePopup();
  yield promisePopupHidden(gURLBar.popup);
  gBrowser.removeTab(tab);
});

add_task(function* searchSuggestions() {
  let engine = yield promiseNewSearchEngine(TEST_ENGINE_BASENAME);
  let oldCurrentEngine = Services.search.currentEngine;
  Services.search.currentEngine = engine;
  Services.prefs.setBoolPref(SUGGEST_ALL_PREF, true);
  Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, true);
  registerCleanupFunction(function() {
    Services.search.currentEngine = oldCurrentEngine;
    Services.prefs.clearUserPref(SUGGEST_ALL_PREF);
    Services.prefs.clearUserPref(SUGGEST_URLBAR_PREF);
  });

  yield promiseAutocompleteResultPopup("foo");
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
    if (child.getAttribute("type").split(/\s+/).indexOf("searchengine") >= 0) {
      Assert.ok(expectedSearches.length > 0);
      let suggestion = expectedSearches.shift();
      Assert.equal(child.label, suggestion + " browser_searchSuggestionEngine searchSuggestionEngine.xml Search",
                   "Result label should be: <search term> <engine name> Search");
    }
  }
  Assert.ok(expectedSearches.length == 0);
  gURLBar.closePopup();
});
