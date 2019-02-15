const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

add_task(async function init() {
  await PlacesUtils.history.clear();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.oneOffSearches", true],
          ["browser.urlbar.suggest.searches", true]],
  });
  let engine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME);
  let oldDefaultEngine = await Services.search.getDefault();
  await Services.search.moveEngine(engine, 0);
  await Services.search.setDefault(engine);
  registerCleanupFunction(async function() {
    await Services.search.setDefault(oldDefaultEngine);

    await PlacesUtils.history.clear();
    // Make sure the popup is closed for the next test.
    await UrlbarTestUtils.promisePopupClose(window);
  });
});

// Presses the Return key when a one-off is selected after selecting a search
// suggestion.
add_task(async function oneOffReturnAfterSuggestion() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  let typedValue = "foo";
  await promiseAutocompleteResultPopup(typedValue, window, true);
  await promiseSuggestionsPresent();
  assertState(0, -1, typedValue);

  // Down to select the first search suggestion.
  EventUtils.synthesizeKey("KEY_ArrowDown");
  assertState(1, -1, "foofoo");

  // Down to select the next search suggestion.
  EventUtils.synthesizeKey("KEY_ArrowDown");
  assertState(2, -1, "foobar");

  // Alt+Down to select the first one-off.
  EventUtils.synthesizeKey("KEY_ArrowDown", {altKey: true});
  assertState(2, 0, "foobar");

  let resultsPromise =
    BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false,
                                   `http://mochi.test:8888/?terms=foobar`);
  EventUtils.synthesizeKey("KEY_Enter");
  await resultsPromise;

  await PlacesUtils.history.clear();
  BrowserTestUtils.removeTab(tab);
});

// Clicks a one-off engine after selecting a search suggestion.
add_task(async function oneOffClickAfterSuggestion() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  let typedValue = "foo";
  await promiseAutocompleteResultPopup(typedValue, window, true);
  await promiseSuggestionsPresent();
  assertState(0, -1, typedValue);

  // Down to select the first search suggestion.
  EventUtils.synthesizeKey("KEY_ArrowDown");
  assertState(1, -1, "foofoo");

  // Down to select the next search suggestion.
  EventUtils.synthesizeKey("KEY_ArrowDown");
  assertState(2, -1, "foobar");

  let oneOffs = UrlbarTestUtils.getOneOffSearchButtons(window).getSelectableButtons(true);
  let resultsPromise =
    BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false,
                                   `http://mochi.test:8888/?terms=foobar`);
  EventUtils.synthesizeMouseAtCenter(oneOffs[0], {});
  await resultsPromise;

  await PlacesUtils.history.clear();
  BrowserTestUtils.removeTab(tab);
});

add_task(async function overridden_engine_not_reused() {
  info("An overridden search suggestion item should not be reused by a search with another engine");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
    let typedValue = "foo";
    await promiseAutocompleteResultPopup(typedValue, window, true);
    await promiseSuggestionsPresent();
    // Down to select the first search suggestion.
    EventUtils.synthesizeKey("KEY_ArrowDown");
    assertState(1, -1, "foofoo");
    // ALT+Down to select the second search engine.
    EventUtils.synthesizeKey("KEY_ArrowDown", {altKey: true});
    EventUtils.synthesizeKey("KEY_ArrowDown", {altKey: true});
    assertState(1, 1, "foofoo");

    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
    let label = result.displayed.action;
    // Run again the query, check the label has been replaced.
    await promiseAutocompleteResultPopup(typedValue, window, true);
    await promiseSuggestionsPresent();
    assertState(0, -1, "foo");
    result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
    Assert.notEqual(result.displayed.action, label, "The label should have been updated");

    BrowserTestUtils.removeTab(tab);
});

function assertState(result, oneOff, textValue = undefined) {
  Assert.equal(UrlbarTestUtils.getSelectedIndex(window), result,
    "Expected result should be selected");
  Assert.equal(UrlbarTestUtils.getOneOffSearchButtons(window).selectedButtonIndex,
    oneOff, "Expected one-off should be selected");
    // TODO Bug 1527946: Fix textValue differences for QuantumBar
    if (UrlbarPrefs.get("quantumbar")) {
      return;
    }
    if (textValue !== undefined) {
      Assert.equal(gURLBar.textValue, textValue, "Expected textValue");
    }
}
