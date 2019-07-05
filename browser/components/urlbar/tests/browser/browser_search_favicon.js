/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that a restyled search result is correctly displayed.
 */

var gOriginalEngine;
var gEngine;
var gRestyleSearchesPref = "browser.urlbar.restyleSearches";

registerCleanupFunction(async () => {
  Services.prefs.clearUserPref(gRestyleSearchesPref);
  await Services.search.setDefault(gOriginalEngine);
  await Services.search.removeEngine(gEngine);
  return PlacesUtils.history.clear();
});

add_task(async function() {
  Services.prefs.setBoolPref(gRestyleSearchesPref, true);

  // This test is sensitive to the mouse position hovering awesome
  // bar elements, so make sure it doesnt
  await EventUtils.synthesizeNativeMouseMove(document.documentElement, 0, 0);
});

add_task(async function() {
  await Services.search.addEngineWithDetails("SearchEngine", {
    method: "GET",
    template: "http://s.example.com/search",
  });
  gEngine = Services.search.getEngineByName("SearchEngine");
  gEngine.addParam("q", "{searchTerms}", null);
  gOriginalEngine = await Services.search.getDefault();
  await Services.search.setDefault(gEngine);

  await PlacesTestUtils.addVisits({
    uri: "http://s.example.com/search?q=foobar&client=1",
    title: "Foo - SearchEngine Search",
  });

  await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:mozilla");

  // The first autocomplete result has the action searchengine, while
  // the second result is the "search favicon" element.
  await promiseAutocompleteResultPopup("foo");
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);

  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.SEARCH);
  Assert.equal(result.displayed.title, "foobar");

  let bundle = Services.strings.createBundle(
    "chrome://global/locale/autocomplete.properties"
  );
  Assert.equal(
    result.displayed.action,
    bundle.formatStringFromName("searchWithEngine", ["SearchEngine"]),
    "Should have the correct action text"
  );

  gBrowser.removeCurrentTab();
});
