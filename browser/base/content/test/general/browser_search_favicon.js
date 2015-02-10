/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let gOriginalEngine;
let gEngine;
let gUnifiedCompletePref = "browser.urlbar.unifiedcomplete";
let gRestyleSearchesPref = "browser.urlbar.restyleSearches";

registerCleanupFunction(() => {
  Services.prefs.clearUserPref(gUnifiedCompletePref);
  Services.prefs.clearUserPref(gRestyleSearchesPref);
  Services.search.currentEngine = gOriginalEngine;
  Services.search.removeEngine(gEngine);
  return PlacesTestUtils.clearHistory();
});

add_task(function*() {
  Services.prefs.setBoolPref(gUnifiedCompletePref, true);
  Services.prefs.setBoolPref(gRestyleSearchesPref, true);
});

add_task(function*() {

  Services.search.addEngineWithDetails("SearchEngine", "", "", "",
                                       "GET", "http://s.example.com/search");
  gEngine = Services.search.getEngineByName("SearchEngine");
  gEngine.addParam("q", "{searchTerms}", null);
  gOriginalEngine = Services.search.currentEngine;
  Services.search.currentEngine = gEngine;

  let uri = NetUtil.newURI("http://s.example.com/search?q=foo&client=1");
  yield PlacesTestUtils.addVisits({ uri: uri, title: "Foo - SearchEngine Search" });

  let tab = gBrowser.selectedTab = gBrowser.addTab("about:mozilla", {animate: false});
  yield promiseTabLoaded(gBrowser.selectedTab);

  // The first autocomplete result has the action searchengine, while
  // the second result is the "search favicon" element.
  yield promiseAutocompleteResultPopup("foo");
  let result = gURLBar.popup.richlistbox.children[1];

  isnot(result, null, "Expect a search result");
  is(result.getAttribute("type"), "search favicon", "Expect correct `type` attribute");

  is_element_visible(result._title, "Title element should be visible");
  is_element_visible(result._extraBox, "Extra box element should be visible");

  is(result._extraBox.pack, "start", "Extra box element should start after the title");
  let iconElem = result._extraBox.nextSibling;
  is_element_visible(iconElem,
                     "The element containing the magnifying glass icon should be visible");
  ok(iconElem.classList.contains("ac-result-type-keyword"),
     "That icon should have the same class use for `keyword` results");

  is_element_visible(result._url, "URL element should be visible");
  is(result._url.textContent, "Search with SearchEngine");

  gBrowser.removeCurrentTab();
});
