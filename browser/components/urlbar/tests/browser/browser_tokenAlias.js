/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This test checks "@" search engine aliases ("token aliases") in the urlbar.

"use strict";

const ALIAS = "@test";
const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

add_task(async function init() {
  // Add a default engine with suggestions, to avoid hitting the network when
  // fetching them.
  let defaultEngine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME
  );
  defaultEngine.alias = "@default";
  let oldDefaultEngine = await Services.search.getDefault();
  Services.search.setDefault(defaultEngine);
  let engine = await Services.search.addEngineWithDetails("Test", {
    alias: ALIAS,
    template: "http://example.com/?search={searchTerms}",
  });
  registerCleanupFunction(async function() {
    await Services.search.removeEngine(engine);
    Services.search.setDefault(oldDefaultEngine);
  });

  // Search results aren't shown in quantumbar unless search suggestions are
  // enabled.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.searches", true]],
  });
});

// Simple test that tries different variations of an alias, without reverting
// the urlbar value in between.
add_task(async function testNoRevert() {
  await doSimpleTest(false);
});

// Simple test that tries different variations of an alias, reverting the urlbar
// value in between.
add_task(async function testRevert() {
  await doSimpleTest(true);
});

// An alias should be recognized and highlighted even when there are spaces
// before it.
add_task(async function spacesBeforeAlias() {
  gURLBar.search("     " + ALIAS);
  await UrlbarTestUtils.promiseSearchComplete(window);
  await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
  await assertAlias(true);

  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );
});

// An alias in the middle of a string should not be recognized or highlighted.
add_task(async function charsBeforeAlias() {
  gURLBar.search("not an alias " + ALIAS);
  await UrlbarTestUtils.promiseSearchComplete(window);
  await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
  await assertAlias(false);

  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );
});

// In a search string that starts with a restriction character followed by an
// alias, the alias should be neither recognized nor highlighted.
add_task(async function restrictionCharBeforeAlias() {
  gURLBar.search(UrlbarTokenizer.RESTRICT.BOOKMARK + " " + ALIAS);
  await UrlbarTestUtils.promiseSearchComplete(window);
  await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
  await assertAlias(false);

  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );
});

// Aliases are case insensitive, and the alias in the result uses the case that
// the user typed in the input.  Make sure that searching with an alias using a
// weird case still causes the alias to be highlighted.
add_task(async function aliasCase() {
  let alias = "@TeSt";
  gURLBar.search(alias);
  await UrlbarTestUtils.promiseSearchComplete(window);
  await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
  await assertAlias(true, alias);

  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );
});

// Even when the heuristic result is a search engine result with an alias, if
// the urlbar value does not match that result, then no alias substring in the
// urlbar should be highlighted.  This is the case when the user uses an alias
// to perform a search: The popup closes (preserving the results in it), the
// urlbar value changes to the URL of the search results page, and the search
// results page is loaded.
add_task(async function inputDoesntMatchHeuristicResult() {
  // Do a search using the alias.
  let searchString = `${ALIAS} aaa`;
  gURLBar.search(searchString);
  await UrlbarTestUtils.promiseSearchComplete(window);
  await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
  await assertAlias(true);

  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );

  // Manually set the urlbar value to a string that contains the alias at the
  // beginning but is not the alias.
  let value = `${ALIAS}xxx`;
  gURLBar.value = `${ALIAS}xxx`;

  // The alias substring should not be highlighted.
  Assert.equal(gURLBar.untrimmedValue, value);
  Assert.ok(gURLBar.untrimmedValue.includes(ALIAS));
  assertHighlighted(false, ALIAS);

  // Do another search using the alias.
  searchString = `${ALIAS} bbb`;
  gURLBar.search(searchString);
  await UrlbarTestUtils.promiseSearchComplete(window);
  await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
  await assertAlias(true);

  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );

  // Manually set the urlbar value to a string that contains the alias, but not
  // at the beginning and is not the same as the search string.
  value = `bbb ${ALIAS}`;
  gURLBar.value = `bbb ${ALIAS}`;

  // The alias substring should not be highlighted.
  Assert.equal(gURLBar.untrimmedValue, value);
  Assert.ok(gURLBar.untrimmedValue.includes(ALIAS));
  assertHighlighted(false, ALIAS);

  // Reset for the next test.
  gURLBar.search("");
});

// Selecting a non-heuristic (non-first) search engine result with an alias and
// empty query should put the alias in the urlbar and highlight it.
add_task(async function nonHeuristicAliases() {
  // Get the list of token alias engines (those with aliases that start with
  // "@").
  let tokenEngines = [];
  for (let engine of await Services.search.getEngines()) {
    let aliases = [];
    if (engine.alias) {
      aliases.push(engine.alias);
    }
    aliases.push(...engine.wrappedJSObject._internalAliases);
    let tokenAliases = aliases.filter(a => a.startsWith("@"));
    if (tokenAliases.length) {
      tokenEngines.push({ engine, tokenAliases });
    }
  }
  if (!tokenEngines.length) {
    Assert.ok(true, "No token alias engines, skipping task.");
    return;
  }
  info(
    "Got token alias engines: " + tokenEngines.map(({ engine }) => engine.name)
  );

  // Populate the results with the list of token alias engines by searching for
  // "@".
  gURLBar.search("@");
  await UrlbarTestUtils.promiseSearchComplete(window);
  await UrlbarTestUtils.waitForAutocompleteResultAt(
    window,
    tokenEngines.length - 1
  );
  // Key down to select each result in turn.  The urlbar value should be set to
  // each alias, and each should be highlighted.
  for (let { tokenAliases } of tokenEngines) {
    let alias = tokenAliases[0];
    EventUtils.synthesizeKey("KEY_ArrowDown");
    assertHighlighted(true, alias);
  }

  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );
});

// Aliases that don't start with @ shouldn't be highlighted.
add_task(async function nonTokenAlias() {
  let alias = "nontokenalias";
  let engine = Services.search.getEngineByName("Test");
  engine.alias = "nontokenalias";
  Assert.equal(engine.alias, alias);

  gURLBar.search(alias);
  await UrlbarTestUtils.promiseSearchComplete(window);
  await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
  await assertFirstResultIsAlias(true, alias);
  assertHighlighted(false);

  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );

  engine.alias = ALIAS;
});

// Clicking on an @ alias offer (an @ alias with an empty search string) in the
// popup should fill it in the urlbar input.
add_task(async function clickAndFillAlias() {
  // Do a search for "@" to show all the @ aliases.
  gURLBar.search("@");
  await UrlbarTestUtils.promiseSearchComplete(window);

  // Find our test engine in the results.  It's probably last, but for
  // robustness don't assume it is.
  let testEngineItem;
  for (let i = 0; !testEngineItem; i++) {
    let details = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    if (details.searchParams && details.searchParams.keyword == ALIAS) {
      testEngineItem = await UrlbarTestUtils.waitForAutocompleteResultAt(
        window,
        i
      );
    }
  }

  // Click it.
  EventUtils.synthesizeMouseAtCenter(testEngineItem, {});

  // A new search will start and its result should be the alias.
  await UrlbarTestUtils.promiseSearchComplete(window);
  await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
  await assertAlias(true);

  // The urlbar input value should be the alias followed by a space so that it's
  // ready for the user to start typing.
  Assert.equal(gURLBar.value, `${ALIAS} `);

  // Press the enter key a couple of times.  Nothing should happen except a new
  // search will start and its result should be the alias again.  The urlbar
  // should still contain the alias.  An empty search results page should not
  // load.  The test will hang if that happens.
  for (let i = 0; i < 2; i++) {
    EventUtils.synthesizeKey("KEY_Enter");
    await UrlbarTestUtils.promiseSearchComplete(window);
    await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
    await assertAlias(true);
    Assert.equal(gURLBar.value, `${ALIAS} `);
  }

  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );
});

// Pressing enter on an @ alias offer (an @ alias with an empty search string)
// in the popup should fill it in the urlbar input.
add_task(async function enterAndFillAlias() {
  // Do a search for "@" to show all the @ aliases.
  gURLBar.search("@");
  await UrlbarTestUtils.promiseSearchComplete(window);

  // Find our test engine in the results.  It's probably last, but for
  // robustness don't assume it is.
  let index = 0;
  for (; ; index++) {
    let details = await UrlbarTestUtils.getDetailsOfResultAt(window, index);
    if (details.searchParams && details.searchParams.keyword == ALIAS) {
      index++;
      break;
    }
  }

  // Key down to it and press enter.
  EventUtils.synthesizeKey("KEY_ArrowDown", { repeat: index });
  EventUtils.synthesizeKey("KEY_Enter");

  // A new search will start and its result should be the alias.
  await UrlbarTestUtils.promiseSearchComplete(window);
  await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
  await assertAlias(true);

  // The urlbar input value should be the alias followed by a space so that it's
  // ready for the user to start typing.
  Assert.equal(gURLBar.value, `${ALIAS} `);

  // Press the enter key a couple of times.  Nothing should happen except a new
  // search will start and its result should be the alias again.  The urlbar
  // should still contain the alias.  An empty search results page should not
  // load.  The test will hang if that happens.
  for (let i = 0; i < 2; i++) {
    EventUtils.synthesizeKey("KEY_Enter");
    await UrlbarTestUtils.promiseSearchComplete(window);
    await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
    await assertAlias(true);
    Assert.equal(gURLBar.value, `${ALIAS} `);
  }

  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );
});

// Pressing enter on an @ alias autofill should fill it in the urlbar input
// with a trailing space and move the caret at the end.
add_task(async function enterAutofillsAlias() {
  let expectedString = `${ALIAS} `;
  for (let value of [ALIAS.substring(0, ALIAS.length - 1), ALIAS]) {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      waitForFocus,
      value,
      selectionStart: value.length,
      selectionEnd: value.length,
    });
    await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);

    // Press Enter.
    EventUtils.synthesizeKey("KEY_Enter");
    await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);

    // The urlbar input value should be the alias followed by a space so that it's
    // ready for the user to start typing.
    Assert.equal(gURLBar.value, expectedString);
    Assert.equal(gURLBar.selectionStart, expectedString.length);
    Assert.equal(gURLBar.selectionEnd, expectedString.length);
    await assertAlias(true);
  }
  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );
});

async function doSimpleTest(revertBetweenSteps) {
  // When autofill is enabled, searching for "@tes" will autofill to "@test",
  // which gets in the way of this test task, so temporarily disable it.
  Services.prefs.setBoolPref("browser.urlbar.autoFill", false);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.urlbar.autoFill");
  });

  // "@tes" -- not an alias, no highlight
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus: SimpleTest.waitForFocus,
    value: ALIAS.substr(0, ALIAS.length - 1),
    fireInputEvent: true,
  });
  await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
  await assertAlias(false);

  if (revertBetweenSteps) {
    gURLBar.handleRevert();
    gURLBar.blur();
  }

  // "@test" -- alias, highlight
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus: SimpleTest.waitForFocus,
    value: ALIAS,
    fireInputEvent: true,
  });
  await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
  await assertAlias(true);

  if (revertBetweenSteps) {
    gURLBar.handleRevert();
    gURLBar.blur();
  }

  // "@test foo" -- alias, highlight
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus: SimpleTest.waitForFocus,
    value: ALIAS + " foo",
    fireInputEvent: true,
  });
  await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
  await assertAlias(true);

  if (revertBetweenSteps) {
    gURLBar.handleRevert();
    gURLBar.blur();
  }

  // "@test" -- alias, highlight
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus: SimpleTest.waitForFocus,
    value: ALIAS,
    fireInputEvent: true,
  });
  await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
  await assertAlias(true);

  if (revertBetweenSteps) {
    gURLBar.handleRevert();
    gURLBar.blur();
  }

  // "@tes" -- not an alias, no highlight
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus: SimpleTest.waitForFocus,
    value: ALIAS.substr(0, ALIAS.length - 1),
    fireInputEvent: true,
  });
  await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
  await assertAlias(false);

  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );

  Services.prefs.clearUserPref("browser.urlbar.autoFill");
}

async function assertAlias(aliasPresent, expectedAlias = ALIAS) {
  await assertFirstResultIsAlias(aliasPresent, expectedAlias);
  assertHighlighted(aliasPresent, expectedAlias);
}

async function assertFirstResultIsAlias(isAlias, expectedAlias) {
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(
    result.type,
    UrlbarUtils.RESULT_TYPE.SEARCH,
    "Should have the correct type"
  );

  Assert.equal(
    "keyword" in result.searchParams && !!result.searchParams.keyword,
    isAlias,
    "Should have a keyword if expected"
  );
  if (isAlias) {
    Assert.equal(
      result.searchParams.keyword,
      expectedAlias,
      "Should have the correct keyword"
    );
  }
}

function assertHighlighted(highlighted, expectedAlias) {
  let selection = gURLBar.editor.selectionController.getSelection(
    Ci.nsISelectionController.SELECTION_FIND
  );
  Assert.ok(selection);
  if (!highlighted) {
    Assert.equal(selection.rangeCount, 0);
    return;
  }
  Assert.equal(selection.rangeCount, 1);
  let index = gURLBar.value.indexOf(expectedAlias);
  Assert.ok(
    index >= 0,
    `gURLBar.value="${gURLBar.value}" expectedAlias="${expectedAlias}"`
  );
  let range = selection.getRangeAt(0);
  Assert.ok(range);
  Assert.equal(range.startOffset, index);
  Assert.equal(range.endOffset, index + expectedAlias.length);
}

/**
 * This test checks that if an engine is marked as hidden then
 * it should not appear in the popup when using the "@" token alias in the search bar.
 */
add_task(async function hiddenEngine() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus: SimpleTest.waitForFocus,
    value: "@",
    fireInputEvent: true,
  });

  const defaultEngine = await Services.search.getDefault();

  let foundDefaultEngineInPopup = false;

  // Checks that the default engine appears in the urlbar's popup.
  for (let i = 0; i < UrlbarTestUtils.getResultCount(window); i++) {
    let details = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    if (defaultEngine.name == details.searchParams.engine) {
      foundDefaultEngineInPopup = true;
      break;
    }
  }
  Assert.ok(foundDefaultEngineInPopup, "Default engine appears in the popup.");

  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );

  // Checks that a hidden default engine (i.e. an engine removed from
  // a user's search settings) does not appear in the urlbar's popup.
  defaultEngine.hidden = true;
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus: SimpleTest.waitForFocus,
    value: "@",
    fireInputEvent: true,
  });
  foundDefaultEngineInPopup = false;
  for (let i = 0; i < UrlbarTestUtils.getResultCount(window); i++) {
    let details = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    if (defaultEngine.name == details.searchParams.engine) {
      foundDefaultEngineInPopup = true;
      break;
    }
  }
  Assert.ok(
    !foundDefaultEngineInPopup,
    "Hidden default engine does not appear in the popup"
  );

  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );

  defaultEngine.hidden = false;
});

/**
 * This test checks that if an engine is marked as hidden then
 * it should not appear in the popup when using the "@" token alias in the search bar.
 */
add_task(async function hiddenEngine() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus: SimpleTest.waitForFocus,
    value: "@",
    fireInptuEvent: true,
  });

  const defaultEngine = await Services.search.getDefault();

  let foundDefaultEngineInPopup = false;

  // Checks that the default engine appears in the urlbar's popup.
  for (let i = 0; i < UrlbarTestUtils.getResultCount(window); i++) {
    let details = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    if (defaultEngine.name == details.searchParams.engine) {
      foundDefaultEngineInPopup = true;
      break;
    }
  }
  Assert.ok(foundDefaultEngineInPopup, "Default engine appears in the popup.");

  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );

  // Checks that a hidden default engine (i.e. an engine removed from
  // a user's search settings) does not appear in the urlbar's popup.
  defaultEngine.hidden = true;
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus: SimpleTest.waitForFocus,
    value: "@",
    fireInputEvent: true,
  });
  foundDefaultEngineInPopup = false;
  for (let i = 0; i < UrlbarTestUtils.getResultCount(window); i++) {
    let details = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    if (defaultEngine.name == details.searchParams.engine) {
      foundDefaultEngineInPopup = true;
      break;
    }
  }
  Assert.ok(
    !foundDefaultEngineInPopup,
    "Hidden default engine does not appear in the popup"
  );

  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );

  defaultEngine.hidden = false;
});
