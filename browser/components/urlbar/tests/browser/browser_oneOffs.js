/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the one-off search buttons in the urlbar.
 */

"use strict";

const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

let gMaxResults;
let engine;

XPCOMUtils.defineLazyGetter(this, "oneOffSearchButtons", () => {
  return UrlbarTestUtils.getOneOffSearchButtons(window);
});

add_task(async function init() {
  gMaxResults = Services.prefs.getIntPref("browser.urlbar.maxRichResults");

  // Add a search suggestion engine and move it to the front so that it appears
  // as the first one-off.
  engine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME
  );
  await Services.search.moveEngine(engine, 0);

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.separatePrivateDefault.ui.enabled", false],
      ["browser.urlbar.suggest.quickactions", false],
      ["browser.urlbar.shortcuts.quickactions", true],
    ],
  });

  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
    await UrlbarTestUtils.formHistory.clear();
  });

  // Initialize history with enough visits to fill up the view.
  await PlacesUtils.history.clear();
  await UrlbarTestUtils.formHistory.clear();
  for (let i = 0; i < gMaxResults; i++) {
    await PlacesTestUtils.addVisits(
      "http://example.com/browser_urlbarOneOffs.js/?" + i
    );
  }

  // Add some more visits to the last URL added above so that the top-sites view
  // will be non-empty.
  for (let i = 0; i < 5; i++) {
    await PlacesTestUtils.addVisits(
      "http://example.com/browser_urlbarOneOffs.js/?" + (gMaxResults - 1)
    );
  }
  await updateTopSites(sites => {
    return sites && sites[0] && sites[0].url.startsWith("http://example.com/");
  });

  // Move the mouse away from the view so that a result or one-off isn't
  // inadvertently highlighted.  See bug 1659011.
  EventUtils.synthesizeMouse(
    gURLBar.inputField,
    0,
    0,
    { type: "mousemove" },
    window
  );
});

// Opens the view without showing the one-offs.  They should be hidden and arrow
// key selection should work properly.
add_task(async function noOneOffs() {
  // Do a search for "@" since we hide the one-offs in that case.
  let value = "@";
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value,
    fireInputEvent: true,
  });
  await TestUtils.waitForCondition(
    () => !oneOffSearchButtons._rebuilding,
    "Waiting for one-offs to finish rebuilding"
  );

  Assert.equal(
    UrlbarTestUtils.getOneOffSearchButtonsVisible(window),
    false,
    "One-offs should be hidden"
  );
  assertState(-1, -1, value);

  // Get the result count.  We don't particularly care what the results are,
  // just what the count is so that we can key through them all.
  let resultCount = UrlbarTestUtils.getResultCount(window);

  // Key down through all results.
  for (let i = 0; i < resultCount; i++) {
    EventUtils.synthesizeKey("KEY_ArrowDown");
    assertState(i, -1);
  }

  // Key down again.  Nothing should be selected.
  EventUtils.synthesizeKey("KEY_ArrowDown");
  assertState(-1, -1, value);

  // Key down again.  The first result should be selected.
  EventUtils.synthesizeKey("KEY_ArrowDown");
  assertState(0, -1);

  // Key up.  Nothing should be selected.
  EventUtils.synthesizeKey("KEY_ArrowUp");
  assertState(-1, -1, value);

  // Key up through all the results.
  for (let i = resultCount - 1; i >= 0; i--) {
    EventUtils.synthesizeKey("KEY_ArrowUp");
    assertState(i, -1);
  }

  // Key up again.  Nothing should be selected.
  EventUtils.synthesizeKey("KEY_ArrowUp");
  assertState(-1, -1, value);

  await hidePopup();
});

// Opens the top-sites view.  The one-offs should be shown.
add_task(async function topSites() {
  // Do a search that shows top sites.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
    fireInputEvent: true,
  });
  await TestUtils.waitForCondition(
    () => !oneOffSearchButtons._rebuilding,
    "Waiting for one-offs to finish rebuilding"
  );

  // There's one top sites result, the page with a lot of visits from init.
  let resultURL = "example.com/browser_urlbarOneOffs.js/?" + (gMaxResults - 1);
  Assert.equal(UrlbarTestUtils.getResultCount(window), 1, "Result count");

  Assert.equal(
    UrlbarTestUtils.getOneOffSearchButtonsVisible(window),
    true,
    "One-offs are visible"
  );

  assertState(-1, -1, "");

  // Key down into the result.
  EventUtils.synthesizeKey("KEY_ArrowDown");
  assertState(0, -1, resultURL);

  // Key down through each one-off.
  let numButtons = oneOffSearchButtons.getSelectableButtons(true).length;
  for (let i = 0; i < numButtons; i++) {
    EventUtils.synthesizeKey("KEY_ArrowDown");
    assertState(-1, i, "");
  }

  // Key down again.  The selection should go away.
  EventUtils.synthesizeKey("KEY_ArrowDown");
  assertState(-1, -1, "");

  // Key down again.  The result should be selected.
  EventUtils.synthesizeKey("KEY_ArrowDown");
  assertState(0, -1, resultURL);

  // Key back up.  The selection should go away.
  EventUtils.synthesizeKey("KEY_ArrowUp");
  assertState(-1, -1, "");

  // Key up again.  The selection should wrap back around to the one-offs.  Key
  // up through all the one-offs.
  for (let i = numButtons - 1; i >= 0; i--) {
    EventUtils.synthesizeKey("KEY_ArrowUp");
    assertState(-1, i, "");
  }

  // Key up.  The result should be selected.
  EventUtils.synthesizeKey("KEY_ArrowUp");
  assertState(0, -1, resultURL);

  // Key up again.  The selection should go away.
  EventUtils.synthesizeKey("KEY_ArrowUp");
  assertState(-1, -1, "");

  await hidePopup();
});

// Keys up and down through the non-top-sites view, i.e., the view that's shown
// when the input has been edited.
add_task(async function editedView() {
  // Use a typed value that returns the visits added above but that doesn't
  // trigger autofill since that would complicate the test.
  let typedValue = "browser_urlbarOneOffs";
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: typedValue,
    fireInputEvent: true,
  });
  await UrlbarTestUtils.waitForAutocompleteResultAt(window, gMaxResults - 1);
  let heuristicResult = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  assertState(0, -1, typedValue);

  // Key down through each result.  The first result is already selected, which
  // is why gMaxResults - 1 is the correct number of times to do this.
  for (let i = 0; i < gMaxResults - 1; i++) {
    EventUtils.synthesizeKey("KEY_ArrowDown");
    // i starts at zero so that the textValue passed to assertState is correct.
    // But that means that i + 1 is the expected selected index, since initially
    // (when this loop starts) the first result is selected.
    assertState(
      i + 1,
      -1,
      "example.com/browser_urlbarOneOffs.js/?" + (gMaxResults - i - 1)
    );
    Assert.ok(
      !BrowserTestUtils.is_visible(heuristicResult.element.action),
      "The heuristic action should not be visible"
    );
  }

  // Key down through each one-off.
  let numButtons = oneOffSearchButtons.getSelectableButtons(true).length;
  for (let i = 0; i < numButtons; i++) {
    EventUtils.synthesizeKey("KEY_ArrowDown");
    assertState(-1, i, typedValue);
    Assert.equal(
      BrowserTestUtils.is_visible(heuristicResult.element.action),
      !oneOffSearchButtons.selectedButton.classList.contains(
        "search-setting-button"
      ),
      "The heuristic action should be visible when a one-off button is selected"
    );
  }

  // Key down once more.  The selection should wrap around to the first result.
  EventUtils.synthesizeKey("KEY_ArrowDown");
  assertState(0, -1, typedValue);
  Assert.ok(
    BrowserTestUtils.is_visible(heuristicResult.element.action),
    "The heuristic action should be visible"
  );

  // Now key up.  The selection should wrap back around to the one-offs.  Key
  // up through all the one-offs.
  for (let i = numButtons - 1; i >= 0; i--) {
    EventUtils.synthesizeKey("KEY_ArrowUp");
    assertState(-1, i, typedValue);
    Assert.equal(
      BrowserTestUtils.is_visible(heuristicResult.element.action),
      !oneOffSearchButtons.selectedButton.classList.contains(
        "search-setting-button"
      ),
      "The heuristic action should be visible when a one-off button is selected"
    );
  }

  // Key up through each non-heuristic result.
  for (let i = gMaxResults - 2; i >= 0; i--) {
    EventUtils.synthesizeKey("KEY_ArrowUp");
    assertState(
      i + 1,
      -1,
      "example.com/browser_urlbarOneOffs.js/?" + (gMaxResults - i - 1)
    );
    Assert.ok(
      !BrowserTestUtils.is_visible(heuristicResult.element.action),
      "The heuristic action should not be visible"
    );
  }

  // Key up once more.  The heuristic result should be selected.
  EventUtils.synthesizeKey("KEY_ArrowUp");
  assertState(0, -1, typedValue);
  Assert.ok(
    BrowserTestUtils.is_visible(heuristicResult.element.action),
    "The heuristic action should be visible"
  );

  await hidePopup();
});

// Checks that "Search with Current Search Engine" items are updated to "Search
// with One-Off Engine" when a one-off is selected.
add_task(async function searchWith() {
  // Enable suggestions for this subtest so we can check non-heuristic results.
  let oldDefaultEngine = await Services.search.getDefault();
  await Services.search.setDefault(engine);
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.searches", true]],
  });

  let typedValue = "foo";
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: typedValue,
  });
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  assertState(0, -1, typedValue);

  Assert.equal(
    result.displayed.action,
    "Search with " + (await Services.search.getDefault()).name,
    "Sanity check: first result's action text"
  );

  // Alt+Down to the second one-off.  Now the first result and the second
  // one-off should both be selected.
  EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true, repeat: 2 });
  assertState(0, 1, typedValue);

  let engineName = oneOffSearchButtons.selectedButton.engine.name;
  Assert.notEqual(
    engineName,
    (await Services.search.getDefault()).name,
    "Sanity check: Second one-off engine should not be the current engine"
  );
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(
    result.displayed.action,
    "Search with " + engineName,
    "First result's action text should be updated"
  );

  // Check non-heuristic results.
  await hidePopup();
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: typedValue,
  });

  EventUtils.synthesizeKey("KEY_ArrowDown");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  assertState(1, -1, typedValue + "foo");
  Assert.equal(
    result.displayed.action,
    "Search with " + engine.name,
    "Sanity check: second result's action text"
  );
  Assert.ok(!result.heuristic, "The second result is not heuristic.");
  EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true, repeat: 2 });
  assertState(1, 1, typedValue + "foo");

  engineName = oneOffSearchButtons.selectedButton.engine.name;
  Assert.notEqual(
    engineName,
    (await Services.search.getDefault()).name,
    "Sanity check: Second one-off engine should not be the current engine"
  );
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);

  Assert.equal(
    result.displayed.action,
    "Search with " + engineName,
    "Second result's action text should be updated"
  );

  await SpecialPowers.popPrefEnv();
  await Services.search.setDefault(oldDefaultEngine);
  await hidePopup();
});

// Clicks a one-off with an engine.
add_task(async function oneOffClick() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  // We are explicitly using something that looks like a url, to make the test
  // stricter. Even if it looks like a url, we should search.
  let typedValue = "foo.bar";

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: typedValue,
  });
  await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  assertState(0, -1, typedValue);
  let oneOffs = oneOffSearchButtons.getSelectableButtons(true);

  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeMouseAtCenter(oneOffs[0], {});
  await searchPromise;
  Assert.ok(UrlbarTestUtils.isPopupOpen(window), "Urlbar view is still open.");
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: oneOffs[0].engine.name,
    entry: "oneoff",
  });
  await UrlbarTestUtils.exitSearchMode(window, { backspace: true });

  gBrowser.removeTab(gBrowser.selectedTab);
  await UrlbarTestUtils.formHistory.clear();
});

// Presses the Return key when a one-off with an engine is selected.
add_task(async function oneOffReturn() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  // We are explicitly using something that looks like a url, to make the test
  // stricter. Even if it looks like a url, we should search.
  let typedValue = "foo.bar";

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: typedValue,
    fireInputEvent: true,
  });
  await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  assertState(0, -1, typedValue);
  let oneOffs = oneOffSearchButtons.getSelectableButtons(true);

  // Alt+Down to select the first one-off.
  EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
  assertState(0, 0, typedValue);

  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeKey("KEY_Enter");
  await searchPromise;
  Assert.ok(UrlbarTestUtils.isPopupOpen(window), "Urlbar view is still open.");
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: oneOffs[0].engine.name,
    entry: "oneoff",
  });
  await UrlbarTestUtils.exitSearchMode(window, { backspace: true });

  gBrowser.removeTab(gBrowser.selectedTab);
  await UrlbarTestUtils.formHistory.clear();
  await hidePopup();
});

// When all engines and local shortcuts are hidden except for the current
// engine, the one-offs container should be hidden.
add_task(async function allOneOffsHiddenExceptCurrentEngine() {
  // Disable all the engines but the current one, check the oneoffs are
  // hidden and that moving up selects the last match.
  let defaultEngine = await Services.search.getDefault();
  let engines = (await Services.search.getVisibleEngines()).filter(
    e => e.name != defaultEngine.name
  );
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.hiddenOneOffs", engines.map(e => e.name).join(",")],
      ...UrlbarUtils.LOCAL_SEARCH_MODES.map(m => [
        `browser.urlbar.${m.pref}`,
        false,
      ]),
    ],
  });

  let typedValue = "foo";
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: typedValue,
    fireInputEvent: true,
  });
  await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  assertState(0, -1);
  Assert.equal(
    UrlbarTestUtils.getOneOffSearchButtonsVisible(window),
    false,
    "The one-off buttons should be hidden"
  );
  EventUtils.synthesizeKey("KEY_ArrowUp");
  assertState(0, -1);
  await hidePopup();
  await SpecialPowers.popPrefEnv();
});

// The one-offs should be hidden when searching with an "@engine" search engine
// alias.
add_task(async function hiddenWhenUsingSearchAlias() {
  let typedValue = "@example";
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: typedValue,
    fireInputEvent: true,
  });
  await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(
    UrlbarTestUtils.getOneOffSearchButtonsVisible(window),
    false,
    "Should not be showing the one-off buttons"
  );
  await hidePopup();

  typedValue = "not an engine alias";
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: typedValue,
    fireInputEvent: true,
  });
  await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(
    UrlbarTestUtils.getOneOffSearchButtonsVisible(window),
    true,
    "Should be showing the one-off buttons"
  );
  await hidePopup();
});

// Makes sure the local shortcuts exist.
add_task(async function localShortcuts() {
  oneOffSearchButtons.invalidateCache();
  await doLocalShortcutsShownTest();
});

// Clicks a local shortcut button.
add_task(async function localShortcutClick() {
  // We are explicitly using something that looks like a url, to make the test
  // stricter. Even if it looks like a url, we should search.
  let typedValue = "foo.bar";

  oneOffSearchButtons.invalidateCache();
  let rebuildPromise = BrowserTestUtils.waitForEvent(
    oneOffSearchButtons,
    "rebuild"
  );
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: typedValue,
  });
  await rebuildPromise;

  let buttons = oneOffSearchButtons.localButtons;
  Assert.ok(buttons.length, "Sanity check: Local shortcuts exist");

  for (let button of buttons) {
    Assert.ok(button.source, "Sanity check: Button has a source");
    let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
    EventUtils.synthesizeMouseAtCenter(button, {});
    await searchPromise;
    Assert.ok(
      UrlbarTestUtils.isPopupOpen(window),
      "Urlbar view is still open."
    );
    await UrlbarTestUtils.assertSearchMode(window, {
      source: button.source,
      entry: "oneoff",
    });
  }

  await UrlbarTestUtils.exitSearchMode(window, { backspace: true });
  await hidePopup();
});

// Presses the Return key when a local shortcut is selected.
add_task(async function localShortcutReturn() {
  // We are explicitly using something that looks like a url, to make the test
  // stricter. Even if it looks like a url, we should search.
  let typedValue = "foo.bar";

  oneOffSearchButtons.invalidateCache();
  let rebuildPromise = BrowserTestUtils.waitForEvent(
    oneOffSearchButtons,
    "rebuild"
  );
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: typedValue,
  });
  await rebuildPromise;

  let buttons = oneOffSearchButtons.localButtons;
  Assert.ok(buttons.length, "Sanity check: Local shortcuts exist");

  let allButtons = oneOffSearchButtons.getSelectableButtons(false);
  let firstLocalIndex = allButtons.length - buttons.length;

  for (let i = 0; i < buttons.length; i++) {
    let button = buttons[i];

    // Alt+Down enough times to select the button.
    let index = firstLocalIndex + i;
    EventUtils.synthesizeKey("KEY_ArrowDown", {
      altKey: true,
      repeat: index + 1,
    });
    await TestUtils.waitForCondition(
      () => oneOffSearchButtons.selectedButtonIndex == index,
      "Waiting for local shortcut to become selected"
    );

    let expectedSelectedResultIndex = -1;
    let count = UrlbarTestUtils.getResultCount(window);
    if (count > 0) {
      let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
      if (result.heuristic) {
        expectedSelectedResultIndex = 0;
      }
    }
    assertState(expectedSelectedResultIndex, index, typedValue);

    Assert.ok(button.source, "Sanity check: Button has a source");
    let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
    EventUtils.synthesizeKey("KEY_Enter");
    await searchPromise;
    Assert.ok(
      UrlbarTestUtils.isPopupOpen(window),
      "Urlbar view is still open."
    );
    await UrlbarTestUtils.assertSearchMode(window, {
      source: button.source,
      entry: "oneoff",
    });
  }

  await UrlbarTestUtils.exitSearchMode(window, { backspace: true });
  await hidePopup();
});

// With an empty search string, clicking a local shortcut should result in no
// heuristic result.
add_task(async function localShortcutEmptySearchString() {
  oneOffSearchButtons.invalidateCache();
  let rebuildPromise = BrowserTestUtils.waitForEvent(
    oneOffSearchButtons,
    "rebuild"
  );
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });
  await rebuildPromise;

  Assert.equal(
    UrlbarTestUtils.getOneOffSearchButtonsVisible(window),
    true,
    "One-offs are visible"
  );

  let buttons = oneOffSearchButtons.localButtons;
  Assert.ok(buttons.length, "Sanity check: Local shortcuts exist");

  for (let button of buttons) {
    Assert.ok(button.source, "Sanity check: Button has a source");
    let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
    EventUtils.synthesizeMouseAtCenter(button, {});
    await searchPromise;
    Assert.ok(
      UrlbarTestUtils.isPopupOpen(window),
      "Urlbar view is still open."
    );
    Assert.equal(
      UrlbarTestUtils.getOneOffSearchButtonsVisible(window),
      true,
      "One-offs are visible"
    );
    await UrlbarTestUtils.assertSearchMode(window, {
      source: button.source,
      entry: "oneoff",
    });

    let resultCount = UrlbarTestUtils.getResultCount(window);
    if (!resultCount) {
      Assert.equal(
        gURLBar.panel.getAttribute("noresults"),
        "true",
        "Panel has no results, therefore should have noresults attribute"
      );
      continue;
    }
    Assert.ok(
      !gURLBar.panel.hasAttribute("noresults"),
      "Panel has results, therefore should not have noresults attribute"
    );
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
    Assert.ok(!result.heuristic, "The first result should not be heuristic");
  }

  await UrlbarTestUtils.exitSearchMode(window, { backspace: true });

  await hidePopup();
});

// Trigger SearchOneOffs.willHide() outside of SearchOneOffs.__rebuild(). Ensure
// that we always show the correct engines in the one-offs. This effectively
// tests SearchOneOffs._engineInfo.domWasUpdated.
add_task(async function avoidWillHideRace() {
  // We set maxHistoricalSearchSuggestions to 0 since this test depends on
  // UrlbarView calling SearchOneOffs.willHide(). That only happens when the
  // Urlbar is in search mode after a query that returned no results.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.maxHistoricalSearchSuggestions", 0]],
  });

  oneOffSearchButtons.invalidateCache();

  // Accel+K triggers SearchOneOffs.willHide() from UrlbarView instead of from
  // SearchOneOffs.__rebuild.
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeKey("k", { accelKey: true });
  await searchPromise;
  Assert.ok(
    UrlbarTestUtils.getOneOffSearchButtonsVisible(window),
    "One-offs should be visible"
  );
  await UrlbarTestUtils.promisePopupClose(window);

  info("Hide all engines but the test engine.");
  let oldDefaultEngine = await Services.search.getDefault();
  await Services.search.setDefault(engine);
  let engines = (await Services.search.getVisibleEngines()).filter(
    e => e.name != engine.name
  );
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.hiddenOneOffs", engines.map(e => e.name).join(",")],
      ...UrlbarUtils.LOCAL_SEARCH_MODES.map(m => [
        `browser.urlbar.${m.pref}`,
        false,
      ]),
    ],
  });
  Assert.ok(
    !oneOffSearchButtons._engineInfo,
    "_engineInfo should be nulled out."
  );

  // This call to SearchOneOffs.willHide() should repopulate _engineInfo but not
  // rebuild the one-offs. _engineInfo.willHide will be true and thus UrlbarView
  // will not open.
  EventUtils.synthesizeKey("k", { accelKey: true });
  // We can't wait for UrlbarTestUtils.promiseSearchComplete here since we
  // expect the popup will not open. We wait for _engineInfo to be populated
  // instead.
  await BrowserTestUtils.waitForCondition(
    () => !!oneOffSearchButtons._engineInfo,
    "_engineInfo is set."
  );
  Assert.ok(!UrlbarTestUtils.isPopupOpen(window), "The UrlbarView is closed.");
  Assert.equal(
    oneOffSearchButtons._engineInfo.willHide,
    true,
    "_engineInfo should be repopulated and willHide should be true."
  );
  Assert.equal(
    oneOffSearchButtons._engineInfo.domWasUpdated,
    undefined,
    "domWasUpdated should not be populated since we haven't yet tried to rebuild the one-offs."
  );

  // Now search. The view will open and the one-offs will rebuild, although
  // the one-offs will not be shown since there is only one engine.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test",
  });
  Assert.equal(
    oneOffSearchButtons._engineInfo.domWasUpdated,
    true,
    "domWasUpdated should be true"
  );
  Assert.ok(
    !UrlbarTestUtils.getOneOffSearchButtonsVisible(window),
    "One-offs should be hidden since there is only one engine."
  );
  await UrlbarTestUtils.promisePopupClose(window);

  await SpecialPowers.popPrefEnv();
  await Services.search.setDefault(oldDefaultEngine);
  await SpecialPowers.popPrefEnv();
});

// Hides each of the local shortcuts one at a time.  The search buttons should
// automatically rebuild themselves.
add_task(async function individualLocalShortcutsHidden() {
  for (let { pref, source } of UrlbarUtils.LOCAL_SEARCH_MODES) {
    await SpecialPowers.pushPrefEnv({
      set: [[`browser.urlbar.${pref}`, false]],
    });

    let rebuildPromise = BrowserTestUtils.waitForEvent(
      oneOffSearchButtons,
      "rebuild"
    );
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "",
    });
    await rebuildPromise;

    Assert.equal(
      UrlbarTestUtils.getOneOffSearchButtonsVisible(window),
      true,
      "One-offs are visible"
    );

    let buttons = oneOffSearchButtons.localButtons;
    Assert.ok(buttons.length, "Sanity check: Local shortcuts exist");

    let otherModes = UrlbarUtils.LOCAL_SEARCH_MODES.filter(
      m => m.source != source
    );
    Assert.equal(
      buttons.length,
      otherModes.length,
      "Expected number of enabled local shortcut buttons"
    );

    for (let i = 0; i < buttons.length; i++) {
      Assert.equal(
        buttons[i].source,
        otherModes[i].source,
        "Button has the expected source"
      );
    }

    await hidePopup();
    await SpecialPowers.popPrefEnv();
  }
});

// Hides all the local shortcuts at once.
add_task(async function allLocalShortcutsHidden() {
  await SpecialPowers.pushPrefEnv({
    set: UrlbarUtils.LOCAL_SEARCH_MODES.map(m => [
      `browser.urlbar.${m.pref}`,
      false,
    ]),
  });

  let rebuildPromise = BrowserTestUtils.waitForEvent(
    oneOffSearchButtons,
    "rebuild"
  );
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });
  await rebuildPromise;

  Assert.equal(
    UrlbarTestUtils.getOneOffSearchButtonsVisible(window),
    true,
    "One-offs are visible"
  );

  Assert.equal(
    oneOffSearchButtons.localButtons.length,
    0,
    "All local shortcuts should be hidden"
  );

  Assert.greater(
    oneOffSearchButtons.getSelectableButtons(false).filter(b => b.engine)
      .length,
    0,
    "Engine one-offs should not be hidden"
  );

  await hidePopup();
  await SpecialPowers.popPrefEnv();
});

// Hides all the engines but none of the local shortcuts.
add_task(async function localShortcutsShownWhenEnginesHidden() {
  let engines = await Services.search.getVisibleEngines();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.hiddenOneOffs", engines.map(e => e.name).join(",")]],
  });

  let rebuildPromise = BrowserTestUtils.waitForEvent(
    oneOffSearchButtons,
    "rebuild"
  );
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });
  await rebuildPromise;

  Assert.equal(
    UrlbarTestUtils.getOneOffSearchButtonsVisible(window),
    true,
    "One-offs are visible"
  );

  Assert.equal(
    oneOffSearchButtons.localButtons.length,
    UrlbarUtils.LOCAL_SEARCH_MODES.length,
    "All local shortcuts are visible"
  );

  Assert.equal(
    oneOffSearchButtons.getSelectableButtons(false).filter(b => b.engine)
      .length,
    0,
    "All engine one-offs are hidden"
  );

  await hidePopup();
  await SpecialPowers.popPrefEnv();
});

/**
 * Checks that the local shortcuts are shown correctly.
 */
async function doLocalShortcutsShownTest() {
  let rebuildPromise = BrowserTestUtils.waitForEvent(
    oneOffSearchButtons,
    "rebuild"
  );
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "doLocalShortcutsShownTest",
  });
  await rebuildPromise;

  let buttons = oneOffSearchButtons.localButtons;
  Assert.equal(buttons.length, 4, "Expected number of local shortcuts");

  let expectedSource;
  let seenIDs = new Set();
  for (let button of buttons) {
    Assert.ok(
      !seenIDs.has(button.id),
      "Should not have already seen button.id"
    );
    seenIDs.add(button.id);
    switch (button.id) {
      case "urlbar-engine-one-off-item-bookmarks":
        expectedSource = UrlbarUtils.RESULT_SOURCE.BOOKMARKS;
        break;
      case "urlbar-engine-one-off-item-tabs":
        expectedSource = UrlbarUtils.RESULT_SOURCE.TABS;
        break;
      case "urlbar-engine-one-off-item-history":
        expectedSource = UrlbarUtils.RESULT_SOURCE.HISTORY;
        break;
      case "urlbar-engine-one-off-item-actions":
        expectedSource = UrlbarUtils.RESULT_SOURCE.ACTIONS;
        break;
      default:
        Assert.ok(false, `Unexpected local shortcut ID: ${button.id}`);
        break;
    }
    Assert.equal(button.source, expectedSource, "Expected button.source");
  }

  await hidePopup();
}

function assertState(result, oneOff, textValue = undefined) {
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(window),
    result,
    "Expected result should be selected"
  );
  Assert.equal(
    oneOffSearchButtons.selectedButtonIndex,
    oneOff,
    "Expected one-off should be selected"
  );
  if (textValue !== undefined) {
    Assert.equal(gURLBar.value, textValue, "Expected value");
  }
}

function hidePopup() {
  return UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeKey("KEY_Escape");
  });
}
