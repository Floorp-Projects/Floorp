/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for restrictions set through UrlbarQueryContext.sources.
 */

testEngine_setup();

add_task(async function test_restrictions() {
  await PlacesTestUtils.addVisits([
    { uri: "http://history.com/", title: "match" },
  ]);
  await PlacesUtils.bookmarks.insert({
    url: "http://bookmark.com/",
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "match",
  });
  await UrlbarProviderOpenTabs.registerOpenTab(
    "http://openpagematch.com/",
    0,
    false
  );

  info("Bookmark restrict");
  let results = await get_results({
    sources: [UrlbarUtils.RESULT_SOURCE.BOOKMARKS],
    searchString: "match",
  });
  // Skip the heuristic result.
  Assert.deepEqual(
    results.filter(r => !r.heuristic).map(r => r.payload.url),
    ["http://bookmark.com/"]
  );

  info("History restrict");
  results = await get_results({
    sources: [UrlbarUtils.RESULT_SOURCE.HISTORY],
    searchString: "match",
  });
  // Skip the heuristic result.
  Assert.deepEqual(
    results.filter(r => !r.heuristic).map(r => r.payload.url),
    ["http://history.com/"]
  );

  info("tabs restrict");
  results = await get_results({
    sources: [UrlbarUtils.RESULT_SOURCE.TABS],
    searchString: "match",
  });
  // Skip the heuristic result.
  Assert.deepEqual(
    results.filter(r => !r.heuristic).map(r => r.payload.url),
    ["http://openpagematch.com/"]
  );

  info("search restrict");
  results = await get_results({
    sources: [UrlbarUtils.RESULT_SOURCE.SEARCH],
    searchString: "match",
  });
  Assert.ok(
    !results.some(r => r.payload.engine != SUGGESTIONS_ENGINE_NAME),
    "All the results should be search results"
  );

  info("search restrict should ignore restriction token");
  results = await get_results({
    sources: [UrlbarUtils.RESULT_SOURCE.SEARCH],
    searchString: `${UrlbarTokenizer.RESTRICT.BOOKMARKS} match`,
  });
  Assert.ok(
    !results.some(r => r.payload.engine != SUGGESTIONS_ENGINE_NAME),
    "All the results should be search results"
  );
  Assert.equal(
    results[0].payload.query,
    `${UrlbarTokenizer.RESTRICT.BOOKMARKS} match`,
    "The restriction token should be ignored and not stripped"
  );

  info("search restrict with other engine");
  results = await get_results({
    sources: [UrlbarUtils.RESULT_SOURCE.SEARCH],
    searchString: "match",
    engineName: "Test",
  });
  Assert.ok(
    !results.some(r => r.payload.engine != "Test"),
    "All the results should be search results from the Test engine"
  );
});

async function get_results(test) {
  let controller = UrlbarTestUtils.newMockController();
  let options = {
    allowAutofill: false,
    isPrivate: false,
    maxResults: 10,
    sources: test.sources,
  };
  if (test.engineName) {
    options.searchMode = {
      source: UrlbarUtils.RESULT_SOURCE.SEARCH,
      engineName: test.engineName,
    };
  }
  let queryContext = createContext(test.searchString, options);
  await controller.startQuery(queryContext);
  return queryContext.results;
}
