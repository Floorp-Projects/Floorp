/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests various actions relating to search suggestions and the one-off buttons.
 */

const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";
const TEST_ENGINE2_BASENAME = "searchSuggestionEngine2.xml";

const serverInfo = {
  scheme: "http",
  host: "localhost",
  port: 20709, // Must be identical to what is in searchSuggestionEngine2.xml
};

var gEngine;
var gEngine2;

add_setup(async function () {
  await PlacesUtils.history.clear();
  await UrlbarTestUtils.formHistory.clear();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.searches", true],
      ["browser.urlbar.maxHistoricalSearchSuggestions", 2],
    ],
  });
  gEngine = await SearchTestUtils.promiseNewSearchEngine({
    url: getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME,
  });
  gEngine2 = await SearchTestUtils.promiseNewSearchEngine({
    url: getRootDirectory(gTestPath) + TEST_ENGINE2_BASENAME,
  });
  let oldDefaultEngine = await Services.search.getDefault();
  await Services.search.moveEngine(gEngine2, 0);
  await Services.search.moveEngine(gEngine, 0);
  await Services.search.setDefault(
    gEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
  registerCleanupFunction(async function () {
    await Services.search.setDefault(
      oldDefaultEngine,
      Ci.nsISearchService.CHANGE_REASON_UNKNOWN
    );

    await PlacesUtils.history.clear();
    await UrlbarTestUtils.formHistory.clear();
  });
});

async function withSuggestions(testFn) {
  // First run with remote suggestions, and then run with form history.
  await withSuggestionOnce(false, testFn);
  await withSuggestionOnce(true, testFn);
}

async function withSuggestionOnce(useFormHistory, testFn) {
  if (useFormHistory) {
    // Add foofoo twice so it's more frecent so it appears first so that the
    // order of form history results matches the order of remote suggestion
    // results.
    await UrlbarTestUtils.formHistory.add(["foofoo", "foofoo", "foobar"]);
  }
  await BrowserTestUtils.withNewTab(gBrowser, async () => {
    let value = "foo";
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value,
      fireInputEvent: true,
    });
    let index = await UrlbarTestUtils.promiseSuggestionsPresent(window);
    await assertState({
      inputValue: value,
      resultIndex: 0,
    });
    await withHttpServer(serverInfo, () => {
      return testFn(index, useFormHistory);
    });
  });
  await PlacesUtils.history.clear();
  await UrlbarTestUtils.formHistory.clear();
}

async function selectSecondSuggestion(index, isFormHistory) {
  // Down to select the first search suggestion.
  for (let i = index; i > 0; --i) {
    EventUtils.synthesizeKey("KEY_ArrowDown");
  }
  await assertState({
    inputValue: "foofoo",
    resultIndex: index,
    suggestion: {
      isFormHistory,
    },
  });

  // Down to select the next search suggestion.
  EventUtils.synthesizeKey("KEY_ArrowDown");
  await assertState({
    inputValue: "foobar",
    resultIndex: index + 1,
    suggestion: {
      isFormHistory,
    },
  });
}

// Presses the Return key when a one-off is selected after selecting a search
// suggestion.
add_task(async function test_returnAfterSuggestion() {
  await withSuggestions(async (index, usingFormHistory) => {
    await selectSecondSuggestion(index, usingFormHistory);

    // Alt+Down to select the first one-off.
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
    await assertState({
      inputValue: "foobar",
      resultIndex: index + 1,
      oneOffIndex: 0,
      suggestion: {
        isFormHistory: usingFormHistory,
      },
    });

    let heuristicResult = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
    Assert.ok(
      !BrowserTestUtils.isVisible(heuristicResult.element.action),
      "The heuristic action should not be visible"
    );

    let resultsPromise = UrlbarTestUtils.promiseSearchComplete(window);
    EventUtils.synthesizeKey("KEY_Enter");
    await resultsPromise;
    await UrlbarTestUtils.assertSearchMode(window, {
      engineName: gEngine.name,
      entry: "oneoff",
    });
    await UrlbarTestUtils.exitSearchMode(window, { backspace: true });
  });
});

// Presses the Return key when a non-default one-off is selected after selecting
// a search suggestion.
add_task(async function test_returnAfterSuggestion_nonDefault() {
  await withSuggestions(async (index, usingFormHistory) => {
    await selectSecondSuggestion(index, usingFormHistory);

    // Alt+Down twice to select the second one-off.
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
    await assertState({
      inputValue: "foobar",
      resultIndex: index + 1,
      oneOffIndex: 1,
      suggestion: {
        isFormHistory: usingFormHistory,
      },
    });

    let resultsPromise = UrlbarTestUtils.promiseSearchComplete(window);
    EventUtils.synthesizeKey("KEY_Enter");
    await resultsPromise;
    await UrlbarTestUtils.assertSearchMode(window, {
      engineName: gEngine2.name,
      entry: "oneoff",
    });
    await UrlbarTestUtils.exitSearchMode(window, { backspace: true });
  });
});

// Clicks a one-off engine after selecting a search suggestion.
add_task(async function test_clickAfterSuggestion() {
  await withSuggestions(async (index, usingFormHistory) => {
    await selectSecondSuggestion(index, usingFormHistory);

    let oneOffs =
      UrlbarTestUtils.getOneOffSearchButtons(window).getSelectableButtons(true);
    let resultsPromise = UrlbarTestUtils.promiseSearchComplete(window);
    EventUtils.synthesizeMouseAtCenter(oneOffs[1], {});
    await resultsPromise;
    await UrlbarTestUtils.assertSearchMode(window, {
      engineName: gEngine2.name,
      entry: "oneoff",
    });
    await UrlbarTestUtils.exitSearchMode(window, { backspace: true });
  });
});

// Clicks a non-default one-off engine after selecting a search suggestion.
add_task(async function test_clickAfterSuggestion_nonDefault() {
  await withSuggestions(async (index, usingFormHistory) => {
    await selectSecondSuggestion(index, usingFormHistory);

    let oneOffs =
      UrlbarTestUtils.getOneOffSearchButtons(window).getSelectableButtons(true);
    let resultsPromise = UrlbarTestUtils.promiseSearchComplete(window);
    EventUtils.synthesizeMouseAtCenter(oneOffs[1], {});
    await resultsPromise;
    await UrlbarTestUtils.assertSearchMode(window, {
      engineName: gEngine2.name,
      entry: "oneoff",
    });
    await UrlbarTestUtils.exitSearchMode(window, { backspace: true });
  });
});

// Selects a non-default one-off engine and then clicks a search suggestion.
add_task(async function test_selectOneOffThenSuggestion() {
  await withSuggestions(async (index, usingFormHistory) => {
    // Select a non-default one-off engine.
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
    await assertState({
      inputValue: "foo",
      resultIndex: 0,
      oneOffIndex: 1,
    });

    let heuristicResult = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
    Assert.ok(
      BrowserTestUtils.isVisible(heuristicResult.element.action),
      "The heuristic action should be visible because the result is selected"
    );

    // Now click the second suggestion.
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, index + 1);
    // Note search history results don't change their engine when the selected
    // one-off button changes!
    let resultsPromise = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser,
      false,
      usingFormHistory
        ? `http://mochi.test:8888/?terms=foobar`
        : `http://localhost:20709/?terms=foobar`
    );
    EventUtils.synthesizeMouseAtCenter(result.element.row, {});
    await resultsPromise;
  });
});

add_task(async function overridden_engine_not_reused() {
  info(
    "An overridden search suggestion item should not be reused by a search with another engine"
  );
  await BrowserTestUtils.withNewTab(gBrowser, async () => {
    let typedValue = "foo";
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: typedValue,
      fireInputEvent: true,
    });
    let index = await UrlbarTestUtils.promiseSuggestionsPresent(window);
    // Down to select the first search suggestion.
    for (let i = index; i > 0; --i) {
      EventUtils.synthesizeKey("KEY_ArrowDown");
    }
    await assertState({
      inputValue: "foofoo",
      resultIndex: index,
      suggestion: {
        isFormHistory: false,
      },
    });

    // ALT+Down to select the second search engine.
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
    await assertState({
      inputValue: "foofoo",
      resultIndex: index,
      oneOffIndex: 1,
      suggestion: {
        isFormHistory: false,
      },
    });

    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
    let label = result.displayed.action;
    // Run again the query, check the label has been replaced.
    await UrlbarTestUtils.promisePopupClose(window);
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: typedValue,
      fireInputEvent: true,
    });
    index = await UrlbarTestUtils.promiseSuggestionsPresent(window);
    await assertState({
      inputValue: "foo",
      resultIndex: 0,
    });
    result = await UrlbarTestUtils.getDetailsOfResultAt(window, index);
    Assert.notEqual(
      result.displayed.action,
      label,
      "The label should have been updated"
    );
  });
});

async function assertState({
  resultIndex,
  inputValue,
  oneOffIndex = -1,
  suggestion = null,
}) {
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(window),
    resultIndex,
    "Expected result should be selected"
  );
  Assert.equal(
    UrlbarTestUtils.getOneOffSearchButtons(window).selectedButtonIndex,
    oneOffIndex,
    "Expected one-off should be selected"
  );
  if (inputValue !== undefined) {
    Assert.equal(gURLBar.value, inputValue, "Expected input value");
  }

  if (suggestion) {
    let result = await UrlbarTestUtils.getDetailsOfResultAt(
      window,
      resultIndex
    );
    Assert.equal(
      result.type,
      UrlbarUtils.RESULT_TYPE.SEARCH,
      "Result type should be SEARCH"
    );
    if (suggestion.isFormHistory) {
      Assert.equal(
        result.source,
        UrlbarUtils.RESULT_SOURCE.HISTORY,
        "Result source should be HISTORY"
      );
    } else {
      Assert.equal(
        result.source,
        UrlbarUtils.RESULT_SOURCE.SEARCH,
        "Result source should be SEARCH"
      );
    }
    Assert.equal(
      typeof result.searchParams.suggestion,
      "string",
      "Result should have a suggestion"
    );
    Assert.equal(
      result.searchParams.suggestion,
      suggestion.value || inputValue,
      "Result should have the expected suggestion"
    );
  }
}
