const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

add_task(async function init() {
  await PlacesUtils.history.clear();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.oneOffSearches", true],
          ["browser.urlbar.suggest.searches", true]],
  });
  let engine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME);
  let oldCurrentEngine = Services.search.currentEngine;
  Services.search.moveEngine(engine, 0);
  Services.search.currentEngine = engine;
  registerCleanupFunction(async function() {
    Services.search.currentEngine = oldCurrentEngine;

    await PlacesUtils.history.clear();
    // Make sure the popup is closed for the next test.
    gURLBar.blur();
    Assert.ok(!gURLBar.popup.popupOpen, "popup should be closed");
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

  let oneOffs = gURLBar.popup.oneOffSearchButtons.getSelectableButtons(true);
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

    let label = gURLBar.popup.richlistbox.children[gURLBar.popup.richlistbox.selectedIndex].label;
    // Run again the query, check the label has been replaced.
    await promiseAutocompleteResultPopup(typedValue, window, true);
    await promiseSuggestionsPresent();
    assertState(0, -1, "foo");
    let newLabel = gURLBar.popup.richlistbox.children[1].label;
    Assert.notEqual(newLabel, label, "The label should have been updated");

    BrowserTestUtils.removeTab(tab);
});

function assertState(result, oneOff, textValue = undefined) {
  Assert.equal(gURLBar.popup.selectedIndex, result,
               "Expected result should be selected");
  Assert.equal(gURLBar.popup.oneOffSearchButtons.selectedButtonIndex, oneOff,
               "Expected one-off should be selected");
  if (textValue !== undefined) {
    Assert.equal(gURLBar.textValue, textValue, "Expected textValue");
  }
}

async function hidePopup() {
  EventUtils.synthesizeKey("KEY_Escape");
  await promisePopupHidden(gURLBar.popup);
}
