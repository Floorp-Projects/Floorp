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

add_task(async function init() {
  await PlacesUtils.history.clear();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.searches", true]],
  });
  let engine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME
  );
  let engine2 = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + TEST_ENGINE2_BASENAME
  );
  let oldDefaultEngine = await Services.search.getDefault();
  await Services.search.moveEngine(engine2, 0);
  await Services.search.moveEngine(engine, 0);
  await Services.search.setDefault(engine);
  registerCleanupFunction(async function() {
    await Services.search.setDefault(oldDefaultEngine);

    await PlacesUtils.history.clear();
  });
});

async function withSecondSuggestion(testFn) {
  await BrowserTestUtils.withNewTab(gBrowser, async () => {
    let typedValue = "foo";
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      waitForFocus: SimpleTest.waitForFocus,
      value: typedValue,
      fireInputEvent: true,
    });
    let index = await UrlbarTestUtils.promiseSuggestionsPresent(window);
    assertState(0, -1, typedValue);

    // Down to select the first search suggestion.
    for (let i = index; i > 0; --i) {
      EventUtils.synthesizeKey("KEY_ArrowDown");
    }
    assertState(index, -1, "foofoo");

    // Down to select the next search suggestion.
    EventUtils.synthesizeKey("KEY_ArrowDown");
    assertState(index + 1, -1, "foobar");

    await withHttpServer(serverInfo, () => {
      return testFn(index + 1);
    });
  });
  await PlacesUtils.history.clear();
}

// Presses the Return key when a one-off is selected after selecting a search
// suggestion.
add_task(async function test_returnAfterSuggestion() {
  await withSecondSuggestion(async index => {
    // Alt+Down to select the first one-off.
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
    assertState(index, 0, "foobar");
    let heuristicResult = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
    Assert.ok(
      !BrowserTestUtils.is_visible(heuristicResult.element.action),
      "The heuristic action should not be visible"
    );

    let resultsPromise = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser,
      false,
      `http://mochi.test:8888/?terms=foobar`
    );
    EventUtils.synthesizeKey("KEY_Enter");
    await resultsPromise;
  });
});

// Presses the Return key when a non-default one-off is selected after selecting
// a search suggestion.
add_task(async function test_returnAfterSuggestion_nonDefault() {
  await withSecondSuggestion(async index => {
    // Alt+Down twice to select the second one-off.
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
    assertState(index, 1, "foobar");

    let resultsPromise = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser,
      false,
      `http://localhost:20709/?terms=foobar`
    );
    EventUtils.synthesizeKey("KEY_Enter");
    await resultsPromise;
  });
});

// Clicks a one-off engine after selecting a search suggestion.
add_task(async function test_clickAfterSuggestion() {
  await withSecondSuggestion(async () => {
    let oneOffs = UrlbarTestUtils.getOneOffSearchButtons(
      window
    ).getSelectableButtons(true);
    let resultsPromise = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser,
      false,
      `http://mochi.test:8888/?terms=foobar`
    );
    EventUtils.synthesizeMouseAtCenter(oneOffs[0], {});
    await resultsPromise;
  });
});

// Clicks a non-default one-off engine after selecting a search suggestion.
add_task(async function test_clickAfterSuggestion_nonDefault() {
  await withSecondSuggestion(async () => {
    let oneOffs = UrlbarTestUtils.getOneOffSearchButtons(
      window
    ).getSelectableButtons(true);
    let resultsPromise = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser,
      false,
      `http://localhost:20709/?terms=foobar`
    );
    EventUtils.synthesizeMouseAtCenter(oneOffs[1], {});
    await resultsPromise;
  });
});

// Selects a non-default one-off engine and then selects a search suggestion.
add_task(async function test_selectOneOffThenSuggestion() {
  await BrowserTestUtils.withNewTab(gBrowser, async () => {
    let typedValue = "foo";
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      waitForFocus: SimpleTest.waitForFocus,
      value: typedValue,
      fireInputEvent: true,
    });
    let index = await UrlbarTestUtils.promiseSuggestionsPresent(window);
    assertState(0, -1, typedValue);

    // Select a non-default one-off engine.
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
    assertState(0, 1, "foo");

    let heuristicResult = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
    Assert.ok(
      BrowserTestUtils.is_visible(heuristicResult.element.action),
      "The heuristic action should be visible because the result is selected"
    );

    // Now click the second suggestion.
    await withHttpServer(serverInfo, async () => {
      let result = await UrlbarTestUtils.getDetailsOfResultAt(
        window,
        index + 1
      );

      let resultsPromise = BrowserTestUtils.browserLoaded(
        gBrowser.selectedBrowser,
        false,
        `http://localhost:20709/?terms=foobar`
      );
      EventUtils.synthesizeMouseAtCenter(result.element.row, {});
      await resultsPromise;
    });
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
      waitForFocus: SimpleTest.waitForFocus,
      value: typedValue,
      fireInputEvent: true,
    });
    let index = await UrlbarTestUtils.promiseSuggestionsPresent(window);
    // Down to select the first search suggestion.
    for (let i = index; i > 0; --i) {
      EventUtils.synthesizeKey("KEY_ArrowDown");
    }
    assertState(index, -1, "foofoo");
    // ALT+Down to select the second search engine.
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
    assertState(index, 1, "foofoo");

    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
    let label = result.displayed.action;
    // Run again the query, check the label has been replaced.
    await UrlbarTestUtils.promisePopupClose(window);
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      waitForFocus: SimpleTest.waitForFocus,
      value: typedValue,
      fireInputEvent: true,
    });
    index = await UrlbarTestUtils.promiseSuggestionsPresent(window);
    assertState(0, -1, "foo");
    result = await UrlbarTestUtils.getDetailsOfResultAt(window, index);
    Assert.notEqual(
      result.displayed.action,
      label,
      "The label should have been updated"
    );
  });
});

function assertState(expectedIndex, oneOffIndex, textValue = undefined) {
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(window),
    expectedIndex,
    "Expected result should be selected"
  );
  Assert.equal(
    UrlbarTestUtils.getOneOffSearchButtons(window).selectedButtonIndex,
    oneOffIndex,
    "Expected one-off should be selected"
  );
  if (textValue !== undefined) {
    Assert.equal(gURLBar.value, textValue, "Expected textValue");
  }
}
