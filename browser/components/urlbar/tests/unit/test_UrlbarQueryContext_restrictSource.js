/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for restrictions set through UrlbarQueryContext.sources.
 */

add_task(async function setup() {
  let engine = await addTestSuggestionsEngine();
  let oldDefaultEngine = await Services.search.getDefault();
  Services.search.setDefault(engine);
  registerCleanupFunction(async () =>
    Services.search.setDefault(oldDefaultEngine)
  );
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "unifiedComplete",
  "@mozilla.org/autocomplete/search;1?name=unifiedcomplete",
  "nsIAutoCompleteSearch"
);

add_task(async function test_restrictions() {
  await PlacesTestUtils.addVisits([
    { uri: "http://history.com/", title: "match" },
  ]);
  await PlacesUtils.bookmarks.insert({
    url: "http://bookmark.com/",
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "match",
  });
  await UrlbarProviderOpenTabs.registerOpenTab("http://openpagematch.com/");

  info("Bookmark restrict");
  let results = await get_results({
    sources: [UrlbarUtils.RESULT_SOURCE.BOOKMARKS],
    searchString: "match",
  });
  // Skip the heuristic result.
  Assert.deepEqual(results.filter(r => !r.heuristic).map(r => r.payload.url), [
    "http://bookmark.com/",
  ]);

  info("History restrict");
  results = await get_results({
    sources: [UrlbarUtils.RESULT_SOURCE.HISTORY],
    searchString: "match",
  });
  // Skip the heuristic result.
  Assert.deepEqual(results.filter(r => !r.heuristic).map(r => r.payload.url), [
    "http://history.com/",
  ]);

  info("tabs restrict");
  results = await get_results({
    sources: [UrlbarUtils.RESULT_SOURCE.TABS],
    searchString: "match",
  });
  // Skip the heuristic result.
  Assert.deepEqual(results.filter(r => !r.heuristic).map(r => r.payload.url), [
    "http://openpagematch.com/",
  ]);

  info("search restrict");
  results = await get_results({
    sources: [UrlbarUtils.RESULT_SOURCE.SEARCH],
    searchString: "match",
  });
  Assert.ok(
    !results.some(r => r.payload.engine != "engine-suggestions.xml"),
    "All the results should be search results"
  );
});

async function get_results(test) {
  let controller = UrlbarTestUtils.newMockController();
  let queryContext = createContext(test.searchString, {
    allowAutofill: false,
    isPrivate: false,
    maxResults: 10,
    sources: test.sources,
  });
  await controller.startQuery(queryContext);
  info(JSON.stringify(queryContext.results));
  return queryContext.results;
}
