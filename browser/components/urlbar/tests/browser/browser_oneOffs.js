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

  Services.prefs.setBoolPref(
    "browser.search.separatePrivateDefault.ui.enabled",
    false
  );
  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
    await UrlbarTestUtils.formHistory.clear();
    Services.prefs.clearUserPref(
      "browser.search.separatePrivateDefault.ui.enabled"
    );
    Services.prefs.clearUserPref("browser.urlbar.update2");
    Services.prefs.clearUserPref("browser.urlbar.update2.oneOffsRefresh");
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

  // Get the result count.  We don't care what the results are, just what the
  // count is so that we can key through them all.
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

// The same as the previous task but with update 2 enabled.  Opens the view
// without showing the one-offs.  Makes sure they're hidden and that arrow key
// selection works properly.
add_task(async function noOneOffsUpdate2() {
  // Set the update2 prefs.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.update2", true],
      ["browser.urlbar.update2.localOneOffs", true],
      ["browser.urlbar.update2.oneOffsRefresh", true],
    ],
  });

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
  await SpecialPowers.popPrefEnv();
});

// Opens the top-sites view with update2 enabled.  The one-offs should be shown.
add_task(async function topSitesUpdate2() {
  // Set the update2 prefs.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.update2", true],
      ["browser.urlbar.update2.localOneOffs", true],
      ["browser.urlbar.update2.oneOffsRefresh", true],
    ],
  });

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
  await SpecialPowers.popPrefEnv();
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

  // TODO: (Bug 1658629) We haven't decided what kind of heuristic action text
  // local one-offs should show. This subtest will start failing if we introduce
  // action text for local one-offs, at which point this array and its uses
  // below can be removed.
  const localOneOffIds = [
    "urlbar-engine-one-off-item-bookmarks",
    "urlbar-engine-one-off-item-tabs",
    "urlbar-engine-one-off-item-history",
  ];

  // Key down through each one-off.
  let numButtons = oneOffSearchButtons.getSelectableButtons(true).length;
  for (let i = 0; i < numButtons; i++) {
    EventUtils.synthesizeKey("KEY_ArrowDown");
    assertState(-1, i, typedValue);
    Assert.equal(
      BrowserTestUtils.is_visible(heuristicResult.element.action),
      !oneOffSearchButtons.selectedButton.classList.contains(
        "search-setting-button-compact"
      ) && !localOneOffIds.includes(oneOffSearchButtons.selectedButton.id),
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
        "search-setting-button-compact"
      ) && !localOneOffIds.includes(oneOffSearchButtons.selectedButton.id),
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
  let oldSuggestPref = Services.prefs.getBoolPref(
    "browser.urlbar.suggest.searches"
  );
  await Services.search.setDefault(engine);
  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", true);

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

  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", oldSuggestPref);
  await Services.search.setDefault(oldDefaultEngine);
  await hidePopup();
});

// Clicks a one-off with an engine.
add_task(async function oneOffClick() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  // We are explicitly using something that looks like a url, to make the test
  // stricter. Even if it looks like a url, we should search.
  let typedValue = "foo.bar";

  for (let refresh of [true, false]) {
    UrlbarPrefs.set("update2", refresh);
    UrlbarPrefs.set("update2.oneOffsRefresh", refresh);

    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: typedValue,
    });
    await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
    assertState(0, -1, typedValue);
    let oneOffs = oneOffSearchButtons.getSelectableButtons(true);

    if (refresh) {
      let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
      EventUtils.synthesizeMouseAtCenter(oneOffs[0], {});
      await searchPromise;
      Assert.ok(
        UrlbarTestUtils.isPopupOpen(window),
        "Urlbar view is still open."
      );
      await UrlbarTestUtils.assertSearchMode(window, {
        engineName: oneOffs[0].engine.name,
      });
      await UrlbarTestUtils.exitSearchMode(window, { backspace: true });
    } else {
      let resultsPromise = BrowserTestUtils.browserLoaded(
        gBrowser.selectedBrowser,
        false,
        "http://mochi.test:8888/?terms=foo.bar"
      );
      EventUtils.synthesizeMouseAtCenter(oneOffs[0], {});
      await resultsPromise;
    }
  }

  gBrowser.removeTab(gBrowser.selectedTab);
  await UrlbarTestUtils.formHistory.clear();
});

// Presses the Return key when a one-off with an engine is selected.
add_task(async function oneOffReturn() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  // We are explicitly using something that looks like a url, to make the test
  // stricter. Even if it looks like a url, we should search.
  let typedValue = "foo.bar";

  for (let refresh of [true, false]) {
    UrlbarPrefs.set("update2", refresh);
    UrlbarPrefs.set("update2.oneOffsRefresh", refresh);

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

    if (refresh) {
      let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
      EventUtils.synthesizeKey("KEY_Enter");
      await searchPromise;
      Assert.ok(
        UrlbarTestUtils.isPopupOpen(window),
        "Urlbar view is still open."
      );
      await UrlbarTestUtils.assertSearchMode(window, {
        engineName: oneOffs[0].engine.name,
      });
      await UrlbarTestUtils.exitSearchMode(window, { backspace: true });
    } else {
      let resultsPromise = BrowserTestUtils.browserLoaded(
        gBrowser.selectedBrowser,
        false,
        "http://mochi.test:8888/?terms=foo.bar"
      );
      EventUtils.synthesizeKey("KEY_Enter");
      await resultsPromise;
    }
  }

  gBrowser.removeTab(gBrowser.selectedTab);
  await UrlbarTestUtils.formHistory.clear();
});

// Hidden engines should not appear in the one-offs.
add_task(async function hiddenOneOffs() {
  // Disable all the engines but the current one, check the oneoffs are
  // hidden and that moving up selects the last match.
  let defaultEngine = await Services.search.getDefault();
  let engines = (await Services.search.getVisibleEngines()).filter(
    e => e.name != defaultEngine.name
  );
  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.hiddenOneOffs", engines.map(e => e.name).join(",")]],
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
    getComputedStyle(oneOffSearchButtons.container).display,
    "none",
    "The one-off buttons should be hidden"
  );
  EventUtils.synthesizeKey("KEY_ArrowUp");
  assertState(1, -1);
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

// Makes sure local search mode one-offs don't exist without update2.
add_task(async function localOneOffsWithoutUpdate2() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.update2", false]],
  });

  // Null out _engines so that the one-offs rebuild themselves when the view
  // opens.
  oneOffSearchButtons._engines = null;
  let rebuildPromise = BrowserTestUtils.waitForEvent(
    oneOffSearchButtons,
    "rebuild"
  );
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "localOneOffsWithoutUpdate2",
  });
  await rebuildPromise;

  Assert.equal(oneOffSearchButtons.localButtons.length, 0);
  Assert.equal(
    document.getElementById("urlbar-engine-one-off-item-bookmarks"),
    null,
    "Bookmarks one-off should not exist"
  );
  Assert.equal(
    document.getElementById("urlbar-engine-one-off-item-tabs"),
    null,
    "Tabs one-off should not exist"
  );
  Assert.equal(
    document.getElementById("urlbar-engine-one-off-item-history"),
    null,
    "History one-off should not exist"
  );

  await hidePopup();
  await SpecialPowers.popPrefEnv();
});

// Makes sure local search mode one-offs exist with update2.
add_task(async function localOneOffsWithUpdate2() {
  // Null out _engines so that the one-offs rebuild themselves when the view
  // opens.
  oneOffSearchButtons._engines = null;
  await doLocalOneOffsShownTest();
});

// Clicks a local search mode one-off.
add_task(async function localOneOffClick() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.update2", true],
      ["browser.urlbar.update2.localOneOffs", true],
      ["browser.urlbar.update2.oneOffsRefresh", true],
    ],
  });

  // We are explicitly using something that looks like a url, to make the test
  // stricter. Even if it looks like a url, we should search.
  let typedValue = "foo.bar";

  // Null out _engines so that the one-offs rebuild themselves when the view
  // opens.
  oneOffSearchButtons._engines = null;
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
  Assert.ok(buttons.length, "Sanity check: Local one-offs exist");

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
    });
  }

  await UrlbarTestUtils.exitSearchMode(window, { backspace: true });
  await hidePopup();
  await SpecialPowers.popPrefEnv();
});

// Presses the Return key when a local search mode one-off is selected.
add_task(async function localOneOffReturn() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.update2", true],
      ["browser.urlbar.update2.localOneOffs", true],
      ["browser.urlbar.update2.oneOffsRefresh", true],
    ],
  });

  // We are explicitly using something that looks like a url, to make the test
  // stricter. Even if it looks like a url, we should search.
  let typedValue = "foo.bar";

  // Null out _engines so that the one-offs rebuild themselves when the view
  // opens.
  oneOffSearchButtons._engines = null;
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
  Assert.ok(buttons.length, "Sanity check: Local one-offs exist");

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
      "Waiting for local one-off to become selected"
    );
    assertState(0, index, typedValue);

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
    });
  }

  await UrlbarTestUtils.exitSearchMode(window, { backspace: true });
  await hidePopup();
  await SpecialPowers.popPrefEnv();
});

// With an empty search string, clicking a local one-off should result in no
// heuristic result.
add_task(async function localOneOffEmptySearchString() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.update2", true],
      ["browser.urlbar.update2.localOneOffs", true],
      ["browser.urlbar.update2.oneOffsRefresh", true],
    ],
  });

  // Null out _engines so that the one-offs rebuild themselves when the view
  // opens.
  oneOffSearchButtons._engines = null;
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
  Assert.ok(buttons.length, "Sanity check: Local one-offs exist");

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

  gURLBar.setSearchMode({});

  await hidePopup();
  await SpecialPowers.popPrefEnv();
});

// The bookmarks one-off should be hidden when suggest.bookmark is false.
add_task(async function hiddenBookmarksOneOff() {
  await doLocalOneOffsShownTest("suggest.bookmark");
});

// The tabs one-off should be hidden when suggest.openpage is false.
add_task(async function hiddenTabsOneOff() {
  await doLocalOneOffsShownTest("suggest.openpage");
});

// The history one-off should be hidden when suggest.history is false.
add_task(async function hiddenHistoryOneOff() {
  await doLocalOneOffsShownTest("suggest.history");
});

/**
 * Checks that the local one-offs are shown correctly.
 *
 * @param {boolean} prefToDisable
 *   If given, this should be one of the `browser.urlbar.suggest` prefs.  It
 *   will be disabled before running the test to make sure the corresponding
 *   button isn't shown.
 */
async function doLocalOneOffsShownTest(prefToDisable = "") {
  let prefs = [
    ["browser.urlbar.update2", true],
    ["browser.urlbar.update2.localOneOffs", true],
    ["browser.urlbar.update2.oneOffsRefresh", true],
  ];
  if (prefToDisable) {
    prefs.push([`browser.urlbar.${prefToDisable}`, false]);
  }
  await SpecialPowers.pushPrefEnv({ set: prefs });

  if (prefToDisable) {
    Assert.equal(
      UrlbarPrefs.get(prefToDisable),
      false,
      `Sanity check: Pref ${prefToDisable} is disabled`
    );
  }

  let rebuildPromise = BrowserTestUtils.waitForEvent(
    oneOffSearchButtons,
    "rebuild"
  );
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "doLocalOneOffsShownTest",
  });
  await rebuildPromise;

  let buttons = oneOffSearchButtons.localButtons;
  Assert.equal(
    buttons.length,
    prefToDisable ? 2 : 3,
    "Expected number of local buttons"
  );

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
      default:
        Assert.ok(false, `Unexpected local button ID: ${button.id}`);
        break;
    }
    Assert.equal(button.source, expectedSource, "Expected button.source");

    switch (prefToDisable) {
      case "suggest.bookmark":
        Assert.notEqual(expectedSource, UrlbarUtils.RESULT_SOURCE.BOOKMARKS);
        break;
      case "suggest.openpage":
        Assert.notEqual(expectedSource, UrlbarUtils.RESULT_SOURCE.TABS);
        break;
      case "suggest.history":
        Assert.notEqual(expectedSource, UrlbarUtils.RESULT_SOURCE.HISTORY);
        break;
      default:
        if (prefToDisable) {
          Assert.ok(false, `Unexpected pref: ${prefToDisable}`);
        }
        break;
    }
  }

  await hidePopup();

  if (prefToDisable) {
    // Run the test again with the pref toggled back to true in order to test
    // that the buttons automatically rebuild on the pref change and that all
    // buttons are shown.
    info(`Running test again with ${prefToDisable} = true`);
    await SpecialPowers.pushPrefEnv({
      set: [[`browser.urlbar.${prefToDisable}`, true]],
    });
    await doLocalOneOffsShownTest();
    await SpecialPowers.popPrefEnv();
  }

  await SpecialPowers.popPrefEnv();
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
