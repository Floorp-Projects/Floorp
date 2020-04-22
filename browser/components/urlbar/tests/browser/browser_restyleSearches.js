/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests restyled search history results.
 */

const RESTYLE_PREF = "browser.urlbar.restyleSearches";
const SUGGEST_URLBAR_PREF = "browser.urlbar.suggest.searches";
const PRIVATE_SEARCH_PREF = "browser.search.separatePrivateDefault.ui.enabled";
// We use the slow engine to ensure that the search history item appears in
// results before the equivalent suggestion, to better approximate real-world
// behavior.
const TEST_ENGINE_BASENAME = "searchSuggestionEngineSlow.xml";
const TEST_ENGINE_2_BASENAME = "searchSuggestionEngine2.xml";
const TEST_QUERY = "test query";

let gSearchUri;

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [SUGGEST_URLBAR_PREF, true],
      [RESTYLE_PREF, true],
      [PRIVATE_SEARCH_PREF, false],
    ],
  });

  let engine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME
  );
  let oldDefaultEngine = await Services.search.getDefault();
  await Services.search.setDefault(engine);

  gSearchUri = (await Services.search.getDefault()).getSubmission(
    `${TEST_QUERY}foo`
  ).uri;

  await PlacesTestUtils.addVisits({
    uri: gSearchUri.spec,
    title: `${TEST_QUERY}foo`,
  });

  registerCleanupFunction(async () => {
    Services.search.setDefault(oldDefaultEngine);
    await PlacesUtils.history.clear();
  });
});

/**
 * Tests that a search history item takes the place of and is restyled as its
 * equivalent search suggestion.
 */
add_task(async function restyleSearches() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus: SimpleTest.waitForFocus,
    value: TEST_QUERY,
  });

  Assert.equal(UrlbarTestUtils.getResultCount(window), 3);
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.SEARCH);
  Assert.equal(result.displayed.title, `${TEST_QUERY}foo`);
  Assert.ok(
    result.searchParams.isSearchHistory,
    "This result should be a restyled search history result."
  );
  Assert.equal(result.searchParams.suggestion, `${TEST_QUERY}foo`);
  Assert.equal(
    result.displayed.action,
    UrlbarUtils.strings.formatStringFromName("searchWithEngine", [
      "searchSuggestionEngineSlow.xml",
    ]),
    "Should have the correct action text"
  );

  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 2);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.SEARCH);
  Assert.equal(result.displayed.title, `${TEST_QUERY}bar`);
  Assert.equal(result.searchParams.suggestion, `${TEST_QUERY}bar`);
  Assert.ok(
    !result.searchParams.isSearchHistory,
    "This result should be a normal search suggestion."
  );
  Assert.equal(
    result.displayed.action,
    UrlbarUtils.strings.formatStringFromName("searchWithEngine", [
      "searchSuggestionEngineSlow.xml",
    ]),
    "Should have the correct action text"
  );

  await UrlbarTestUtils.promisePopupClose(window);
});

/**
 * Tests that equivalent search history and search suggestions both appear when
 * the restyleSearches pref is off.
 */
add_task(async function displaySearchHistoryAndSuggestions() {
  await SpecialPowers.pushPrefEnv({
    set: [[RESTYLE_PREF, false]],
  });

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus: SimpleTest.waitForFocus,
    value: TEST_QUERY,
  });

  // We should now also have the search history result.
  Assert.equal(UrlbarTestUtils.getResultCount(window), 4);

  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.SEARCH);
  Assert.equal(result.displayed.title, `${TEST_QUERY}foo`);
  Assert.equal(result.searchParams.suggestion, `${TEST_QUERY}foo`);
  Assert.ok(
    !result.searchParams.isSearchHistory,
    "This result should be a normal search suggestion."
  );
  Assert.equal(
    result.displayed.action,
    UrlbarUtils.strings.formatStringFromName("searchWithEngine", [
      "searchSuggestionEngineSlow.xml",
    ]),
    "Should have the correct action text"
  );

  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 2);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.SEARCH);
  Assert.equal(result.displayed.title, `${TEST_QUERY}bar`);
  Assert.equal(result.searchParams.suggestion, `${TEST_QUERY}bar`);
  Assert.ok(
    !result.searchParams.isSearchHistory,
    "This result should be a normal search suggestion."
  );
  Assert.equal(
    result.displayed.action,
    UrlbarUtils.strings.formatStringFromName("searchWithEngine", [
      "searchSuggestionEngineSlow.xml",
    ]),
    "Should have the correct action text"
  );

  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 3);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.URL);
  Assert.equal(result.source, UrlbarUtils.RESULT_SOURCE.HISTORY);
  Assert.equal(result.displayed.title, `${TEST_QUERY}foo`);
  Assert.equal(result.url, gSearchUri.spec);

  await SpecialPowers.popPrefEnv();
  await UrlbarTestUtils.promisePopupClose(window);
});

/**
 * Tests that search history results from non-default engines do not replace
 * equivalent search suggestions from different engines.
 *
 * Note: The search history result from the original engine should still appear
 *       restyled.
 */
add_task(async function alternateEngine() {
  let engine2 = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + TEST_ENGINE_2_BASENAME
  );
  let previousTestEngine = await Services.search.getDefault();
  await Services.search.setDefault(engine2);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus: SimpleTest.waitForFocus,
    value: TEST_QUERY,
  });

  // We should have the search history item from the original engine, restyled
  // as a suggestion.
  Assert.equal(UrlbarTestUtils.getResultCount(window), 4);

  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.SEARCH);
  Assert.equal(result.displayed.title, `${TEST_QUERY}foo`);
  Assert.equal(result.searchParams.suggestion, `${TEST_QUERY}foo`);
  Assert.ok(
    !result.searchParams.isSearchHistory,
    "This result should be a normal search suggestion."
  );
  Assert.equal(
    result.displayed.action,
    UrlbarUtils.strings.formatStringFromName("searchWithEngine", [
      "browser_searchSuggestionEngine2 searchSuggestionEngine2.xml",
    ]),
    "Should have the correct action text"
  );

  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 2);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.SEARCH);
  Assert.equal(result.displayed.title, `${TEST_QUERY}bar`);
  Assert.equal(result.searchParams.suggestion, `${TEST_QUERY}bar`);
  Assert.ok(
    !result.searchParams.isSearchHistory,
    "This result should be a normal search suggestion."
  );
  Assert.equal(
    result.displayed.action,
    UrlbarUtils.strings.formatStringFromName("searchWithEngine", [
      "browser_searchSuggestionEngine2 searchSuggestionEngine2.xml",
    ]),
    "Should have the correct action text"
  );

  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 3);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.SEARCH);
  Assert.equal(result.displayed.title, `${TEST_QUERY}foo`);
  Assert.ok(
    result.searchParams.isSearchHistory,
    "This result should be a restyled search history result."
  );
  Assert.equal(result.searchParams.suggestion, `${TEST_QUERY}foo`);
  // Note the comparison with searchSuggestionEngineSlow.xml, not
  // searchSuggestionEngine2.xml.
  Assert.equal(
    result.displayed.action,
    UrlbarUtils.strings.formatStringFromName("searchWithEngine", [
      "searchSuggestionEngineSlow.xml",
    ]),
    "Should have the correct action text"
  );

  await Services.search.setDefault(previousTestEngine);
  await UrlbarTestUtils.promisePopupClose(window);
});

/**
 * Tests that when the user types part of a search history URL but not the query
 * in that URL, we show a normal history result that is not restyled.
 */
add_task(async function onlyRestyleWhenQueriesMatch() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus: SimpleTest.waitForFocus,
    value: "mochi",
  });

  // We should now also have the search history result.
  Assert.equal(UrlbarTestUtils.getResultCount(window), 4);

  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.SEARCH);
  Assert.equal(result.displayed.title, "mochifoo");
  Assert.equal(result.searchParams.suggestion, "mochifoo");
  Assert.ok(
    !result.searchParams.isSearchHistory,
    "This result should be a normal search suggestion."
  );
  Assert.equal(
    result.displayed.action,
    UrlbarUtils.strings.formatStringFromName("searchWithEngine", [
      "searchSuggestionEngineSlow.xml",
    ]),
    "Should have the correct action text"
  );

  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 2);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.SEARCH);
  Assert.equal(result.displayed.title, "mochibar");
  Assert.equal(result.searchParams.suggestion, "mochibar");
  Assert.ok(
    !result.searchParams.isSearchHistory,
    "This result should be a normal search suggestion."
  );
  Assert.equal(
    result.displayed.action,
    UrlbarUtils.strings.formatStringFromName("searchWithEngine", [
      "searchSuggestionEngineSlow.xml",
    ]),
    "Should have the correct action text"
  );

  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 3);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.URL);
  Assert.equal(result.source, UrlbarUtils.RESULT_SOURCE.HISTORY);
  Assert.equal(result.displayed.title, `${TEST_QUERY}foo`);
  Assert.equal(result.url, gSearchUri.spec);

  await UrlbarTestUtils.promisePopupClose(window);
});

/**
 * Tests that search history items that do not exactly match a search suggestion
 * URL still appear in results, do not replace suggestions, and are not restyled.
 *
 * Note: This test should run last because it adds a new history item.
 */
add_task(async function searchHistoryNotExactMatch() {
  let irrelevantSearchUri = `${gSearchUri.spec}&irrelevantParameter=1`;

  // This history item will be removed when the setup test runs the cleanup
  // function.
  await PlacesTestUtils.addVisits({
    uri: irrelevantSearchUri,
    title: `${TEST_QUERY}foo irrelevant`,
  });

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus: SimpleTest.waitForFocus,
    value: TEST_QUERY,
  });

  // We should have the irrelevant history result, but the history result that
  // is an exact match should have replaced the equivalent suggestion.
  Assert.equal(UrlbarTestUtils.getResultCount(window), 4);

  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.SEARCH);
  Assert.equal(result.displayed.title, `${TEST_QUERY}foo`);
  Assert.ok(
    result.searchParams.isSearchHistory,
    "This result should be a restyled search history result."
  );
  Assert.equal(result.searchParams.suggestion, `${TEST_QUERY}foo`);
  Assert.equal(
    result.displayed.action,
    UrlbarUtils.strings.formatStringFromName("searchWithEngine", [
      "searchSuggestionEngineSlow.xml",
    ]),
    "Should have the correct action text"
  );

  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 2);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.SEARCH);
  Assert.equal(result.displayed.title, `${TEST_QUERY}bar`);
  Assert.equal(result.searchParams.suggestion, `${TEST_QUERY}bar`);
  Assert.ok(
    !result.searchParams.isSearchHistory,
    "This result should be a normal search suggestion."
  );
  Assert.equal(
    result.displayed.action,
    UrlbarUtils.strings.formatStringFromName("searchWithEngine", [
      "searchSuggestionEngineSlow.xml",
    ]),
    "Should have the correct action text"
  );

  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 3);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.URL);
  Assert.equal(result.source, UrlbarUtils.RESULT_SOURCE.HISTORY);
  Assert.equal(result.displayed.title, `${TEST_QUERY}foo irrelevant`);
  Assert.equal(result.url, irrelevantSearchUri);

  await UrlbarTestUtils.promisePopupClose(window);
});
