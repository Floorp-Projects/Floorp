/* eslint-disable mozilla/no-arbitrary-setTimeout */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This tests that when a search is stopped due to the user selecting a result,
 * the view doesn't update after that.
 */

"use strict";

const TEST_ENGINE_BASENAME = "searchSuggestionEngineSlow.xml";

// This should match the `timeout` query param used in the suggestions URL in
// the test engine.
const TEST_ENGINE_SUGGESTIONS_TIMEOUT = 3000;

// The number of suggestions returned by the test engine.
const TEST_ENGINE_NUM_EXPECTED_RESULTS = 2;

add_setup(async function () {
  await PlacesUtils.history.clear();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.searches", true]],
  });
  // Add a test search engine that returns suggestions on a delay.
  let engine = await SearchTestUtils.installOpenSearchEngine({
    url: getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME,
    setAsDefault: true,
  });
  await Services.search.moveEngine(engine, 0);
  registerCleanupFunction(async () => {
    await PlacesUtils.history.clear();
  });
});

add_task(async function mainTest() {
  // Open a tab that will match the search string below so that we're guaranteed
  // to have more than one result (the heuristic result) so that we can change
  // the selected result.  We open a tab instead of adding a page in history
  // because open tabs are kept in a memory SQLite table, so open-tab results
  // are more likely than history results to be fetched before our slow search
  // suggestions.  This is important when the test runs on slow debug builds on
  // slow machines.
  await BrowserTestUtils.withNewTab("http://example.com/", async () => {
    await BrowserTestUtils.withNewTab("about:blank", async () => {
      // Do an initial search.  There should be 4 results: heuristic, open tab,
      // and the two suggestions.
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: "amp",
      });
      await TestUtils.waitForCondition(() => {
        return (
          UrlbarTestUtils.getResultCount(window) ==
          2 + TEST_ENGINE_NUM_EXPECTED_RESULTS
        );
      });

      // Type a character to start a new search.  The new search should still
      // match the open tab so that the open-tab result appears again.
      EventUtils.synthesizeKey("l");

      // There should be 2 results immediately: heuristic and open tab.
      await TestUtils.waitForCondition(() => {
        return UrlbarTestUtils.getResultCount(window) == 2;
      });

      // Before the search completes, change the selected result.  Pressing only
      // the down arrow key ends up selecting the first one-off on Linux debug
      // builds on the infrastructure for some reason, so arrow back up to
      // select the heuristic result again.  The important thing is to change
      // the selection.  It doesn't matter which result ends up selected.
      EventUtils.synthesizeKey("KEY_ArrowDown");
      EventUtils.synthesizeKey("KEY_ArrowUp");

      // Wait for the new search to complete.  It should be canceled due to the
      // selection change, but it should still complete.
      await UrlbarTestUtils.promiseSearchComplete(window);

      // To make absolutely sure the suggestions don't appear after the search
      // completes, wait a bit.
      await new Promise(r =>
        setTimeout(r, 1 + TEST_ENGINE_SUGGESTIONS_TIMEOUT)
      );

      // The heuristic result should reflect the new search, "ampl".
      let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
      Assert.equal(
        result.type,
        UrlbarUtils.RESULT_TYPE.SEARCH,
        "Should have the correct result type"
      );
      Assert.equal(
        result.searchParams.query,
        "ampl",
        "Should have the correct query"
      );

      // None of the other results should be "ampl" suggestions, i.e., amplfoo
      // and amplbar should not be in the results.
      let count = UrlbarTestUtils.getResultCount(window);
      for (let i = 1; i < count; i++) {
        result = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
        Assert.ok(
          result.type != UrlbarUtils.RESULT_TYPE.SEARCH ||
            !["amplfoo", "amplbar"].includes(result.searchParams.suggestion),
          "Suggestions should not contain the typed l char"
        );
      }
    });
  });
});
