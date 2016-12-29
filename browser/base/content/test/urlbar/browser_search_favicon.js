var gOriginalEngine;
var gEngine;
var gRestyleSearchesPref = "browser.urlbar.restyleSearches";

registerCleanupFunction(() => {
  Services.prefs.clearUserPref(gRestyleSearchesPref);
  Services.search.currentEngine = gOriginalEngine;
  Services.search.removeEngine(gEngine);
  return PlacesTestUtils.clearHistory();
});

add_task(function*() {
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

  yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:mozilla");

  // The first autocomplete result has the action searchengine, while
  // the second result is the "search favicon" element.
  yield promiseAutocompleteResultPopup("foo");
  let result = gURLBar.popup.richlistbox.children[1];

  isnot(result, null, "Expect a search result");
  is(result.getAttribute("type"), "searchengine", "Expect correct `type` attribute");

  let titleHbox = result._titleText.parentNode.parentNode;
  ok(titleHbox.classList.contains("ac-title"), "Title hbox sanity check");
  is_element_visible(titleHbox, "Title element should be visible");

  let urlHbox = result._urlText.parentNode.parentNode;
  ok(urlHbox.classList.contains("ac-url"), "URL hbox sanity check");
  is_element_hidden(urlHbox, "URL element should be hidden");

  let actionHbox = result._actionText.parentNode.parentNode;
  ok(actionHbox.classList.contains("ac-action"), "Action hbox sanity check");
  is_element_hidden(actionHbox, "Action element should be hidden because it is not selected");
  is(result._actionText.textContent, "Search with SearchEngine", "Action text should be as expected");

  gBrowser.removeCurrentTab();
});
