/* eslint-disable mozilla/no-arbitrary-setTimeout */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_ENGINE_BASENAME = "searchSuggestionEngineSlow.xml";

// This should match the `timeout` query param used in the suggestions URL in
// the test engine.
const TEST_ENGINE_SUGGESTIONS_TIMEOUT = 1000;

// The number of suggestions returned by the test engine.
const TEST_ENGINE_NUM_EXPECTED_RESULTS = 2;

add_task(async function init() {
  await PlacesUtils.history.clear();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.searches", true]],
  });
  // Add a test search engine that returns suggestions on a delay.
  let engine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME);
  let oldCurrentEngine = Services.search.currentEngine;
  Services.search.moveEngine(engine, 0);
  Services.search.currentEngine = engine;
  registerCleanupFunction(async () => {
    Services.search.currentEngine = oldCurrentEngine;
    await PlacesUtils.history.clear();
    // Make sure the popup is closed for the next test.
    gURLBar.blur();
    Assert.ok(!gURLBar.popup.popupOpen, "popup should be closed");
  });
});

add_task(async function mainTest() {
  // Trigger an initial search.  Use the "$" token to restrict matches to search
  // suggestions.
  await promiseAutocompleteResultPopup("$ test", window);
  await promiseSuggestionsPresent("Waiting for initial suggestions");

  // Now synthesize typing a character.  promiseAutocompleteResultPopup doesn't
  // work in this case because it causes the popup to close and re-open with the
  // new input text.
  await new Promise(r => EventUtils.synthesizeKey("x", {}, window, r));

  // At this point, a new search starts, the autocomplete's _invalidate is
  // called, _appendCurrentResult is called, and matchCount remains 3 from the
  // previous search (the heuristic result plus the two search suggestions).
  // The suggestion results should not outwardly change, however, since the
  // autocomplete controller should still be returning the initial suggestions.
  // What has changed, though, is the search string, which is kept in the
  // results' ac-text attributes.  Call waitForAutocompleteResultAt to wait for
  // the results' ac-text to be set to the new search string, "testx", to make
  // sure the autocomplete has seen the new search.
  await waitForAutocompleteResultAt(TEST_ENGINE_NUM_EXPECTED_RESULTS);

  // Now press the Down arrow key to change the selection.
  await new Promise(r => EventUtils.synthesizeKey("VK_DOWN", {}, window, r));

  // Changing the selection should have stopped the new search triggered by
  // typing the "x" character.  Wait a bit to make sure it really stopped.
  await new Promise(r => setTimeout(r, 2 * TEST_ENGINE_SUGGESTIONS_TIMEOUT));

  // Both of the suggestion results should retain their initial values,
  // "testfoo" and "testbar".  They should *not* be "testxfoo" and "textxbar".

  // + 1 for the heuristic result
  let numExpectedResults = TEST_ENGINE_NUM_EXPECTED_RESULTS + 1;
  let results = gURLBar.popup.richlistbox.children;
  let numActualResults = Array.reduce(results, (memo, result) => {
    if (!result.collapsed) {
      memo++;
    }
    return memo;
  }, 0);
  Assert.equal(numActualResults, numExpectedResults);

  let expectedSuggestions = ["testfoo", "testbar"];
  for (let i = 0; i < TEST_ENGINE_NUM_EXPECTED_RESULTS; i++) {
    // + 1 to skip the heuristic result
    let item = gURLBar.popup.richlistbox.children[i + 1];
    let action = item._parseActionUrl(item.getAttribute("url"));
    Assert.ok(action);
    Assert.equal(action.type, "searchengine");
    Assert.ok("searchSuggestion" in action.params);
    Assert.equal(action.params.searchSuggestion, expectedSuggestions[i]);
  }
});
