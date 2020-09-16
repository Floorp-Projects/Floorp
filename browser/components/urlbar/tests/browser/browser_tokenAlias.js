/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This test checks "@" search engine aliases ("token aliases") in the urlbar.

"use strict";

const ALIAS = "@test";
const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

let testEngine;

add_task(async function init() {
  // This test requires update2.  See also browser_tokenAlias_legacy.js.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.update2", true],
      ["browser.urlbar.update2.localOneOffs", true],
      ["browser.urlbar.update2.oneOffsRefresh", true],
    ],
  });

  // Add a default engine with suggestions, to avoid hitting the network when
  // fetching them.
  let defaultEngine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME
  );
  defaultEngine.alias = "@default";
  let oldDefaultEngine = await Services.search.getDefault();
  Services.search.setDefault(defaultEngine);
  testEngine = await Services.search.addEngineWithDetails("Test", {
    alias: ALIAS,
    template: "http://example.com/?search={searchTerms}",
  });
  registerCleanupFunction(async function() {
    await Services.search.removeEngine(testEngine);
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

async function doSimpleTest(revertBetweenSteps) {
  // When autofill is enabled, searching for "@tes" will autofill to "@test",
  // which gets in the way of this test task, so temporarily disable it.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.autoFill", false]],
  });

  // "@tes" -- not an alias, no search mode
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: ALIAS.substr(0, ALIAS.length - 1),
  });
  await UrlbarTestUtils.assertSearchMode(window, null);
  Assert.equal(
    gURLBar.value,
    ALIAS.substr(0, ALIAS.length - 1),
    "value should be alias substring"
  );

  if (revertBetweenSteps) {
    gURLBar.handleRevert();
    gURLBar.blur();
  }

  // "@test" -- alias but no trailing space, no search mode
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: ALIAS,
  });
  await UrlbarTestUtils.assertSearchMode(window, null);
  Assert.equal(gURLBar.value, ALIAS, "value should be alias");

  if (revertBetweenSteps) {
    gURLBar.handleRevert();
    gURLBar.blur();
  }

  // "@test " -- alias with trailing space, search mode
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: ALIAS + " ",
  });
  // Wait for the second new search that starts when search mode is entered.
  await UrlbarTestUtils.promiseSearchComplete(window);
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: testEngine.name,
    entry: "typed",
  });
  Assert.equal(gURLBar.value, "", "value should be empty");
  await UrlbarTestUtils.exitSearchMode(window);

  if (revertBetweenSteps) {
    gURLBar.handleRevert();
    gURLBar.blur();
  }

  // "@test foo" -- alias, search mode
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: ALIAS + " foo",
  });
  // Wait for the second new search that starts when search mode is entered.
  await UrlbarTestUtils.promiseSearchComplete(window);
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: testEngine.name,
    entry: "typed",
  });
  Assert.equal(gURLBar.value, "foo", "value should be query");
  await UrlbarTestUtils.exitSearchMode(window);

  if (revertBetweenSteps) {
    gURLBar.handleRevert();
    gURLBar.blur();
  }

  // "@test " -- alias with trailing space, search mode
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: ALIAS + " ",
  });
  // Wait for the second new search that starts when search mode is entered.
  await UrlbarTestUtils.promiseSearchComplete(window);
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: testEngine.name,
    entry: "typed",
  });
  Assert.equal(gURLBar.value, "", "value should be empty");
  await UrlbarTestUtils.exitSearchMode(window);

  if (revertBetweenSteps) {
    gURLBar.handleRevert();
    gURLBar.blur();
  }

  // "@test" -- alias but no trailing space, no highlight
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: ALIAS,
  });
  await UrlbarTestUtils.assertSearchMode(window, null);
  Assert.equal(gURLBar.value, ALIAS, "value should be alias");

  if (revertBetweenSteps) {
    gURLBar.handleRevert();
    gURLBar.blur();
  }

  // "@tes" -- not an alias, no highlight
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: ALIAS.substr(0, ALIAS.length - 1),
  });
  await UrlbarTestUtils.assertSearchMode(window, null);
  Assert.equal(
    gURLBar.value,
    ALIAS.substr(0, ALIAS.length - 1),
    "value should be alias substring"
  );

  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );

  await SpecialPowers.popPrefEnv();
}

// An alias should be recognized even when there are spaces before it, and
// search mode should be entered.
add_task(async function spacesBeforeAlias() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "     " + ALIAS + " ",
  });
  // Wait for the second new search that starts when search mode is entered.
  await UrlbarTestUtils.promiseSearchComplete(window);
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: testEngine.name,
    entry: "typed",
  });
  Assert.equal(gURLBar.value, "", "value should be empty");
  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );
});

// An alias in the middle of a string should not be recognized and search mode
// should not be entered.
add_task(async function charsBeforeAlias() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "not an alias " + ALIAS + " ",
  });
  await UrlbarTestUtils.assertSearchMode(window, null);
  Assert.equal(
    gURLBar.value,
    "not an alias " + ALIAS + " ",
    "value should be unchanged"
  );

  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );
});

// While already in search mode, an alias should not be recognized.
add_task(async function alreadyInSearchMode() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });
  await UrlbarTestUtils.enterSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
  });

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: ALIAS + " ",
  });

  // Search mode source should still be bookmarks.
  await UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    entry: "oneoff",
  });
  Assert.equal(gURLBar.value, ALIAS + " ", "value should be unchanged");

  // Exit search mode, but first remove the value in the input.  Since the value
  // is "alias ", we'd actually immediately re-enter search mode otherwise.
  gURLBar.value = "";

  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );
});

// Types a space while typing an alias to ensure we stop autofilling.
add_task(async function spaceWhileTypingAlias() {
  let value = ALIAS.substring(0, ALIAS.length - 1);
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value,
    selectionStart: value.length,
    selectionEnd: value.length,
  });
  Assert.equal(gURLBar.value, ALIAS + " ", "Alias should be autofilled");

  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeKey(" ");
  await searchPromise;

  Assert.equal(gURLBar.value, value + " ", "Alias should not be autofilled");
  await UrlbarTestUtils.assertSearchMode(window, null);

  await UrlbarTestUtils.promisePopupClose(window);
});

// Aliases are case insensitive.  Make sure that searching with an alias using a
// weird case still causes the alias to be recognized and search mode entered.
add_task(async function aliasCase() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "@TeSt ",
  });
  // Wait for the second new search that starts when search mode is entered.
  await UrlbarTestUtils.promiseSearchComplete(window);
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: testEngine.name,
    entry: "typed",
  });
  Assert.equal(gURLBar.value, "", "value should be empty");
  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );
});

// Same as previous but with a query.
add_task(async function aliasCase_query() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "@tEsT query",
  });
  // Wait for the second new search that starts when search mode is entered.
  await UrlbarTestUtils.promiseSearchComplete(window);
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: testEngine.name,
    entry: "typed",
  });
  Assert.equal(gURLBar.value, "query", "value should be query");
  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );
});

// Selecting a non-heuristic (non-first) search engine result with an alias and
// empty query should put the alias in the urlbar and highlight it.
// Also checks that internal aliases appear with the "@" keyword.
add_task(async function nonHeuristicAliases() {
  // Get the list of token alias engines (those with aliases that start with
  // "@").
  let tokenEngines = [];
  for (let engine of await Services.search.getEngines()) {
    let aliases = [];
    if (engine.alias) {
      aliases.push(engine.alias);
    }
    aliases.push(...engine.aliases);
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
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "@",
  });
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

// Clicking on an @ alias offer (an @ alias with an empty search string) in the
// view should enter search mode.
add_task(async function clickAndFillAlias() {
  // Do a search for "@" to show all the @ aliases.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "@",
  });

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
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeMouseAtCenter(testEngineItem, {});
  await searchPromise;

  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: testEngineItem.result.payload.engine,
    entry: "keywordoffer",
  });

  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );
});

// Pressing enter on an @ alias offer (an @ alias with an empty search string)
// in the view should enter search mode.
add_task(async function enterAndFillAlias() {
  // Do a search for "@" to show all the @ aliases.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "@",
  });

  // Find our test engine in the results.  It's probably last, but for
  // robustness don't assume it is.
  let details;
  let index = 0;
  for (; ; index++) {
    details = await UrlbarTestUtils.getDetailsOfResultAt(window, index);
    if (details.searchParams && details.searchParams.keyword == ALIAS) {
      index++;
      break;
    }
  }

  // Key down to it and press enter.
  EventUtils.synthesizeKey("KEY_ArrowDown", { repeat: index });
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeKey("KEY_Enter");
  await searchPromise;

  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: details.searchParams.engine,
    entry: "keywordoffer",
  });

  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );
});

// Pressing Enter on an @ alias autofill should enter search mode.
add_task(async function enterAutofillsAlias() {
  for (let value of [ALIAS.substring(0, ALIAS.length - 1), ALIAS]) {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value,
      selectionStart: value.length,
      selectionEnd: value.length,
    });

    // Press Enter.
    let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
    EventUtils.synthesizeKey("KEY_Enter");
    await searchPromise;

    await UrlbarTestUtils.assertSearchMode(window, {
      engineName: testEngine.name,
      entry: "keywordoffer",
    });

    await UrlbarTestUtils.exitSearchMode(window);
  }
  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );
});

// Pressing Right on an @ alias autofill should enter search mode.
add_task(async function rightEntersSearchMode() {
  for (let value of [ALIAS.substring(0, ALIAS.length - 1), ALIAS]) {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value,
      selectionStart: value.length,
      selectionEnd: value.length,
    });

    let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
    EventUtils.synthesizeKey("KEY_ArrowRight");
    await searchPromise;

    await UrlbarTestUtils.assertSearchMode(window, {
      engineName: testEngine.name,
      entry: "typed",
    });
    Assert.equal(gURLBar.value, "", "value should be empty");
    await UrlbarTestUtils.exitSearchMode(window);
  }

  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );
});

/**
 * This test checks that if an engine is marked as hidden then
 * it should not appear in the popup when using the "@" token alias in the search bar.
 */
add_task(async function hiddenEngine() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
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
 * This test checks that if an engines alias is not prefixed with
 * @ it still appears in the popup when using the "@" token
 * alias in the search bar.
 */
add_task(async function nonPrefixedKeyword() {
  let name = "Custom";
  let alias = "customkeyword";
  let engine = await Services.search.addEngineWithDetails(name, {
    alias,
    template: "http://example.com/?search={searchTerms}",
  });

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "@",
  });

  let foundEngineInPopup = false;

  // Checks that the default engine appears in the urlbar's popup.
  for (let i = 0; i < UrlbarTestUtils.getResultCount(window); i++) {
    let details = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    if (details.searchParams.engine === name) {
      foundEngineInPopup = true;
      break;
    }
  }
  Assert.ok(foundEngineInPopup, "Custom engine appears in the popup.");

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "@" + alias,
  });

  let keywordOfferResult = await UrlbarTestUtils.getDetailsOfResultAt(
    window,
    0
  );

  Assert.equal(
    keywordOfferResult.searchParams.engine,
    name,
    "The first result should be a keyword search result with the correct engine."
  );

  await Services.search.removeEngine(engine);
});

async function assertFirstResultIsAlias(isAlias, expectedAlias) {
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(
    result.type,
    UrlbarUtils.RESULT_TYPE.SEARCH,
    "Should have the correct type"
  );

  if (isAlias) {
    Assert.equal(
      result.searchParams.keyword,
      expectedAlias,
      "Payload keyword should be the alias"
    );
  } else {
    Assert.notEqual(
      result.searchParams.keyword,
      expectedAlias,
      "Payload keyword should be absent or not the alias"
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
