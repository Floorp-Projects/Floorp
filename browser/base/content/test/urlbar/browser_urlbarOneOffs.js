const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

let gMaxResults;

add_task(async function init() {
  Services.prefs.setBoolPref("browser.urlbar.oneOffSearches", true);
  gMaxResults = Services.prefs.getIntPref("browser.urlbar.maxRichResults");

  // Add a search suggestion engine and move it to the front so that it appears
  // as the first one-off.
  let engine = await promiseNewSearchEngine(TEST_ENGINE_BASENAME);
  Services.search.moveEngine(engine, 0);

  registerCleanupFunction(async function() {
    await hidePopup();
    await PlacesUtils.history.clear();
  });

  await PlacesUtils.history.clear();

  let visits = [];
  for (let i = 0; i < gMaxResults; i++) {
    visits.push({
      uri: makeURI("http://example.com/browser_urlbarOneOffs.js/?" + i),
      // TYPED so that the visit shows up when the urlbar's drop-down arrow is
      // pressed.
      transition: Ci.nsINavHistoryService.TRANSITION_TYPED,
    });
  }
  await PlacesTestUtils.addVisits(visits);
});

// Keys up and down through the history panel, i.e., the panel that's shown when
// there's no text in the textbox.
add_task(async function history() {
  gURLBar.focus();
  EventUtils.synthesizeKey("KEY_ArrowDown");
  await promisePopupShown(gURLBar.popup);
  await waitForAutocompleteResultAt(gMaxResults - 1);

  assertState(-1, -1, "");

  // Key down through each result.
  for (let i = 0; i < gMaxResults; i++) {
    EventUtils.synthesizeKey("KEY_ArrowDown");
    assertState(i, -1,
      "example.com/browser_urlbarOneOffs.js/?" + (gMaxResults - i - 1));
  }

  // Key down through each one-off.
  let numButtons =
    gURLBar.popup.oneOffSearchButtons.getSelectableButtons(true).length;
  for (let i = 0; i < numButtons; i++) {
    EventUtils.synthesizeKey("KEY_ArrowDown");
    assertState(-1, i, "");
  }

  // Key down once more.  Nothing should be selected.
  EventUtils.synthesizeKey("KEY_ArrowDown");
  assertState(-1, -1, "");

  // Once more.  The first result should be selected.
  EventUtils.synthesizeKey("KEY_ArrowDown");
  assertState(0, -1,
    "example.com/browser_urlbarOneOffs.js/?" + (gMaxResults - 1));

  // Now key up.  Nothing should be selected again.
  EventUtils.synthesizeKey("KEY_ArrowUp");
  assertState(-1, -1, "");

  // Key up through each one-off.
  for (let i = numButtons - 1; i >= 0; i--) {
    EventUtils.synthesizeKey("KEY_ArrowUp");
    assertState(-1, i, "");
  }

  // Key right through each one-off.
  for (let i = 1; i < numButtons; i++) {
    EventUtils.synthesizeKey("KEY_ArrowRight");
    assertState(-1, i, "");
  }

  // Key left through each one-off.
  for (let i = numButtons - 2; i >= 0; i--) {
    EventUtils.synthesizeKey("KEY_ArrowLeft");
    assertState(-1, i, "");
  }

  // Key up through each result.
  for (let i = gMaxResults - 1; i >= 0; i--) {
    EventUtils.synthesizeKey("KEY_ArrowUp");
    assertState(i, -1,
      "example.com/browser_urlbarOneOffs.js/?" + (gMaxResults - i - 1));
  }

  // Key up once more.  Nothing should be selected.
  EventUtils.synthesizeKey("KEY_ArrowUp");
  assertState(-1, -1, "");

  await hidePopup();
});

// Keys up and down through the non-history panel, i.e., the panel that's shown
// when you type something in the textbox.
add_task(async function() {
  // Use a typed value that returns the visits added above but that doesn't
  // trigger autofill since that would complicate the test.
  let typedValue = "browser_urlbarOneOffs";
  await promiseAutocompleteResultPopup(typedValue, window, true);
  await waitForAutocompleteResultAt(gMaxResults - 1);
  assertState(0, -1, typedValue);

  // Key down through each result.  The first result is already selected, which
  // is why gMaxResults - 1 is the correct number of times to do this.
  for (let i = 0; i < gMaxResults - 1; i++) {
    EventUtils.synthesizeKey("KEY_ArrowDown");
    // i starts at zero so that the textValue passed to assertState is correct.
    // But that means that i + 1 is the expected selected index, since initially
    // (when this loop starts) the first result is selected.
    assertState(i + 1, -1,
      "example.com/browser_urlbarOneOffs.js/?" + (gMaxResults - i - 1));
  }

  // Key down through each one-off.
  let numButtons =
    gURLBar.popup.oneOffSearchButtons.getSelectableButtons(true).length;
  for (let i = 0; i < numButtons; i++) {
    EventUtils.synthesizeKey("KEY_ArrowDown");
    assertState(-1, i, typedValue);
  }

  // Key down once more.  The selection should wrap around to the first result.
  EventUtils.synthesizeKey("KEY_ArrowDown");
  assertState(0, -1, typedValue);

  // Now key up.  The selection should wrap back around to the one-offs.  Key
  // up through all the one-offs.
  for (let i = numButtons - 1; i >= 0; i--) {
    EventUtils.synthesizeKey("KEY_ArrowUp");
    assertState(-1, i, typedValue);
  }

  // Key up through each non-heuristic result.
  for (let i = gMaxResults - 2; i >= 0; i--) {
    EventUtils.synthesizeKey("KEY_ArrowUp");
    assertState(i + 1, -1,
      "example.com/browser_urlbarOneOffs.js/?" + (gMaxResults - i - 1));
  }

  // Key up once more.  The heuristic result should be selected.
  EventUtils.synthesizeKey("KEY_ArrowUp");
  assertState(0, -1, typedValue);

  await hidePopup();
});

// Checks that "Search with Current Search Engine" items are updated to "Search
// with One-Off Engine" when a one-off is selected.
add_task(async function searchWith() {
  let typedValue = "foo";
  await promiseAutocompleteResultPopup(typedValue);
  await waitForAutocompleteResultAt(0);
  assertState(0, -1, typedValue);

  let item = gURLBar.popup.richlistbox.firstChild;
  Assert.equal(item._actionText.textContent,
               "Search with " + Services.search.currentEngine.name,
               "Sanity check: first result's action text");

  // Alt+Down to the first one-off.  Now the first result and the first one-off
  // should both be selected.
  EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
  assertState(0, 0, typedValue);

  let engineName = gURLBar.popup.oneOffSearchButtons.selectedButton.engine.name;
  Assert.notEqual(engineName, Services.search.currentEngine.name,
                  "Sanity check: First one-off engine should not be " +
                  "the current engine");
  Assert.equal(item._actionText.textContent,
               "Search with " + engineName,
               "First result's action text should be updated");

  await hidePopup();
});

// Clicks a one-off.
add_task(async function oneOffClick() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  // We are explicitly using something that looks like a url, to make the test
  // stricter. Even if it looks like a url, we should search.
  let typedValue = "foo.bar";
  await promiseAutocompleteResultPopup(typedValue);
  await waitForAutocompleteResultAt(1);
  assertState(0, -1, typedValue);

  let oneOffs = gURLBar.popup.oneOffSearchButtons.getSelectableButtons(true);
  let resultsPromise =
    BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false,
                                   "http://mochi.test:8888/?terms=foo.bar");
  EventUtils.synthesizeMouseAtCenter(oneOffs[0], {});
  await resultsPromise;

  gBrowser.removeTab(gBrowser.selectedTab);
});

// Presses the Return key when a one-off is selected.
add_task(async function oneOffReturn() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  // We are explicitly using something that looks like a url, to make the test
  // stricter. Even if it looks like a url, we should search.
  let typedValue = "foo.bar";
  await promiseAutocompleteResultPopup(typedValue, window, true);
  await waitForAutocompleteResultAt(1);
  assertState(0, -1, typedValue);

  // Alt+Down to select the first one-off.
  EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
  assertState(0, 0, typedValue);

  let resultsPromise =
    BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false,
                                   "http://mochi.test:8888/?terms=foo.bar");
  EventUtils.synthesizeKey("KEY_Enter");
  await resultsPromise;

  gBrowser.removeTab(gBrowser.selectedTab);
});

add_task(async function collapsedOneOffs() {
  // Disable all the engines but the current one, check the oneoffs are
  // collapsed and that moving up selects the last match.
  let engines = Services.search.getVisibleEngines()
                               .filter(e => e.name != Services.search.currentEngine.name);
  await SpecialPowers.pushPrefEnv({"set": [
    [ "browser.search.hiddenOneOffs", engines.map(e => e.name).join(",") ]
  ]});

  let typedValue = "foo";
  await promiseAutocompleteResultPopup(typedValue, window, true);
  await waitForAutocompleteResultAt(0);
  assertState(0, -1);
  Assert.ok(gURLBar.popup.oneOffSearchButtons.buttons.collapsed,
    "The one-off buttons should be collapsed");
  EventUtils.synthesizeKey("KEY_ArrowUp");
  assertState(1, -1);
  await hidePopup();
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
