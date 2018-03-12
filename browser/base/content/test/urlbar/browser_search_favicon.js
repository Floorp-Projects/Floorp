var gOriginalEngine;
var gEngine;
var gRestyleSearchesPref = "browser.urlbar.restyleSearches";

registerCleanupFunction(() => {
  Services.prefs.clearUserPref(gRestyleSearchesPref);
  Services.search.currentEngine = gOriginalEngine;
  Services.search.removeEngine(gEngine);
  return PlacesUtils.history.clear();
});

add_task(async function() {
  Services.prefs.setBoolPref(gRestyleSearchesPref, true);

  // This test is sensitive to the mouse position hovering awesome
  // bar elements, so make sure it doesnt
  await EventUtils.synthesizeNativeMouseMove(document.documentElement, 0, 0);
});

add_task(async function() {

  Services.search.addEngineWithDetails("SearchEngine", "", "", "",
                                       "GET", "http://s.example.com/search");
  gEngine = Services.search.getEngineByName("SearchEngine");
  gEngine.addParam("q", "{searchTerms}", null);
  gOriginalEngine = Services.search.currentEngine;
  Services.search.currentEngine = gEngine;

  let uri = NetUtil.newURI("http://s.example.com/search?q=foobar&client=1");
  await PlacesTestUtils.addVisits({ uri, title: "Foo - SearchEngine Search" });

  await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:mozilla");

  // The first autocomplete result has the action searchengine, while
  // the second result is the "search favicon" element.
  await promiseAutocompleteResultPopup("foo");
  let result = await waitForAutocompleteResultAt(1);
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
