/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This test checks "@" search engine aliases ("token aliases") in the urlbar.

"use strict";

const TEST_ALIAS_ENGINE_NAME = "Test";
const ALIAS = "@test";
const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

// We make sure that aliases and search terms are correctly recognized when they
// are separated by each of these different types of spaces and combinations of
// spaces.  U+3000 is the ideographic space in CJK and is commonly used by CJK
// speakers.
const TEST_SPACES = [" ", "\u3000", " \u3000", "\u3000 "];

// Allow more time for Mac machines so they don't time out in verify mode.  See
// bug 1673062.
if (AppConstants.platform == "macosx") {
  requestLongerTimeout(5);
}

add_task(async function init() {
  // Add a default engine with suggestions, to avoid hitting the network when
  // fetching them.
  let defaultEngine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME
  );
  defaultEngine.alias = "@default";
  let oldDefaultEngine = await Services.search.getDefault();
  Services.search.setDefault(defaultEngine);
  await SearchTestUtils.installSearchExtension({
    name: TEST_ALIAS_ENGINE_NAME,
    keyword: ALIAS,
  });

  registerCleanupFunction(async function() {
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
  for (let spaces of TEST_SPACES) {
    info("Testing: " + JSON.stringify({ spaces: codePoints(spaces) }));

    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: ALIAS + spaces,
    });
    // Wait for the second new search that starts when search mode is entered.
    await UrlbarTestUtils.promiseSearchComplete(window);
    await UrlbarTestUtils.assertSearchMode(window, {
      engineName: TEST_ALIAS_ENGINE_NAME,
      entry: "typed",
    });
    Assert.equal(gURLBar.value, "", "value should be empty");
    await UrlbarTestUtils.exitSearchMode(window);

    if (revertBetweenSteps) {
      gURLBar.handleRevert();
      gURLBar.blur();
    }
  }

  // "@test foo" -- alias, search mode
  for (let spaces of TEST_SPACES) {
    info("Testing: " + JSON.stringify({ spaces: codePoints(spaces) }));

    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: ALIAS + spaces + "foo",
    });
    // Wait for the second new search that starts when search mode is entered.
    await UrlbarTestUtils.promiseSearchComplete(window);
    await UrlbarTestUtils.assertSearchMode(window, {
      engineName: TEST_ALIAS_ENGINE_NAME,
      entry: "typed",
    });
    Assert.equal(gURLBar.value, "foo", "value should be query");
    await UrlbarTestUtils.exitSearchMode(window);

    if (revertBetweenSteps) {
      gURLBar.handleRevert();
      gURLBar.blur();
    }
  }

  // "@test " -- alias with trailing space, search mode
  for (let spaces of TEST_SPACES) {
    info("Testing: " + JSON.stringify({ spaces: codePoints(spaces) }));

    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: ALIAS + spaces,
    });
    // Wait for the second new search that starts when search mode is entered.
    await UrlbarTestUtils.promiseSearchComplete(window);
    await UrlbarTestUtils.assertSearchMode(window, {
      engineName: TEST_ALIAS_ENGINE_NAME,
      entry: "typed",
    });
    Assert.equal(gURLBar.value, "", "value should be empty");
    await UrlbarTestUtils.exitSearchMode(window);

    if (revertBetweenSteps) {
      gURLBar.handleRevert();
      gURLBar.blur();
    }
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
  for (let spaces of TEST_SPACES) {
    info("Testing: " + JSON.stringify({ spaces: codePoints(spaces) }));

    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: spaces + ALIAS + spaces,
    });
    // Wait for the second new search that starts when search mode is entered.
    await UrlbarTestUtils.promiseSearchComplete(window);
    await UrlbarTestUtils.assertSearchMode(window, {
      engineName: TEST_ALIAS_ENGINE_NAME,
      entry: "typed",
    });
    Assert.equal(gURLBar.value, "", "value should be empty");
    await UrlbarTestUtils.exitSearchMode(window);
    await UrlbarTestUtils.promisePopupClose(window, () =>
      EventUtils.synthesizeKey("KEY_Escape")
    );
  }
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
  for (let spaces of TEST_SPACES) {
    if (spaces.length != 1) {
      continue;
    }
    info("Testing: " + JSON.stringify({ spaces: codePoints(spaces) }));

    let value = ALIAS.substring(0, ALIAS.length - 1);
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value,
      selectionStart: value.length,
      selectionEnd: value.length,
    });
    Assert.equal(gURLBar.value, ALIAS + " ", "Alias should be autofilled");

    let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
    EventUtils.synthesizeKey(spaces);
    await searchPromise;

    Assert.equal(
      gURLBar.value,
      value + spaces,
      "Alias should not be autofilled"
    );
    await UrlbarTestUtils.assertSearchMode(window, null);

    await UrlbarTestUtils.promisePopupClose(window);
  }
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
    engineName: TEST_ALIAS_ENGINE_NAME,
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
    engineName: TEST_ALIAS_ENGINE_NAME,
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
  // Key down to select each result in turn.  The urlbar should preview search
  // mode for each engine.
  for (let { tokenAliases } of tokenEngines) {
    let alias = tokenAliases[0];
    let engineName = (await UrlbarSearchUtils.engineForAlias(alias)).name;
    EventUtils.synthesizeKey("KEY_ArrowDown");
    let expectedSearchMode = {
      engineName,
      entry: "keywordoffer",
      isPreview: true,
    };
    if (UrlbarUtils.WEB_ENGINE_NAMES.has(engineName)) {
      expectedSearchMode.source = UrlbarUtils.RESULT_SOURCE.SEARCH;
    }
    await UrlbarTestUtils.assertSearchMode(window, expectedSearchMode);
    Assert.ok(!gURLBar.value, "The Urlbar should be empty.");
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
    Assert.equal(
      details.displayed.title,
      `Search with ${details.searchParams.engine}`,
      "The result's title is set correctly."
    );
    Assert.ok(!details.action, "The result should have no action text.");
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
      engineName: TEST_ALIAS_ENGINE_NAME,
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
      engineName: TEST_ALIAS_ENGINE_NAME,
      entry: "typed",
    });
    Assert.equal(gURLBar.value, "", "value should be empty");
    await UrlbarTestUtils.exitSearchMode(window);
  }

  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );
});

// Pressing Tab when an @ alias is autofilled should enter search mode preview.
add_task(async function rightEntersSearchMode() {
  for (let value of [ALIAS.substring(0, ALIAS.length - 1), ALIAS]) {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value,
      selectionStart: value.length,
      selectionEnd: value.length,
    });

    Assert.equal(
      UrlbarTestUtils.getSelectedRowIndex(window),
      -1,
      "There is no selected result."
    );

    EventUtils.synthesizeKey("KEY_Tab");
    Assert.equal(
      UrlbarTestUtils.getSelectedRowIndex(window),
      0,
      "The first result is selected."
    );

    await UrlbarTestUtils.assertSearchMode(window, {
      engineName: TEST_ALIAS_ENGINE_NAME,
      entry: "keywordoffer",
      isPreview: true,
    });
    Assert.equal(gURLBar.value, "", "value should be empty");

    let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
    EventUtils.synthesizeKey("KEY_Enter");
    await searchPromise;

    await UrlbarTestUtils.assertSearchMode(window, {
      engineName: TEST_ALIAS_ENGINE_NAME,
      entry: "keywordoffer",
      isPreview: false,
    });
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
  let extension = await SearchTestUtils.installSearchExtension(
    {
      name,
      keyword: alias,
    },
    true
  );

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

  await extension.unload();
});

// Tests that we show all engines with a token alias that match the search
// string.
add_task(async function multipleMatchingEngines() {
  let extension = await SearchTestUtils.installSearchExtension(
    {
      name: "TestFoo",
      keyword: `${ALIAS}foo`,
    },
    true
  );

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "@te",
    fireInputEvent: true,
  });

  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    2,
    "Two results are shown."
  );
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(window),
    -1,
    "Neither result is selected."
  );
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(result.autofill, "The first result is autofilling.");
  Assert.equal(
    result.searchParams.keyword,
    ALIAS,
    "The autofilled engine is shown first."
  );

  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(
    result.searchParams.keyword,
    `${ALIAS}foo`,
    "The other engine is shown second."
  );

  EventUtils.synthesizeKey("KEY_Tab");
  Assert.equal(UrlbarTestUtils.getSelectedRowIndex(window), 0);
  Assert.equal(gURLBar.value, "", "Urlbar should be empty.");
  EventUtils.synthesizeKey("KEY_Tab");
  Assert.equal(UrlbarTestUtils.getSelectedRowIndex(window), 1);
  Assert.equal(gURLBar.value, "", "Urlbar should be empty.");
  EventUtils.synthesizeKey("KEY_Tab");
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(window),
    -1,
    "Tabbing all the way through the matching engines should return to the input."
  );
  Assert.equal(
    gURLBar.value,
    "@te",
    "Urlbar should contain the search string."
  );

  await extension.unload();
});

// Tests that UrlbarProviderTokenAliasEngines is disabled in search mode.
add_task(async function doNotShowInSearchMode() {
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

  Assert.equal(
    testEngineItem.result.payload.keyword,
    ALIAS,
    "Sanity check: we found our engine."
  );

  // Click it.
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeMouseAtCenter(testEngineItem, {});
  await searchPromise;
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: testEngineItem.result.payload.engine,
    entry: "keywordoffer",
  });

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "@",
    fireInputEvent: true,
  });

  let resultCount = UrlbarTestUtils.getResultCount(window);
  for (let i = 0; i < resultCount; i++) {
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    Assert.ok(
      !result.searchParams.keyword,
      `Result at index ${i} is not a keywordoffer.`
    );
  }
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

/**
 * Returns an array of code points in the given string.  Each code point is
 * returned as a hexidecimal string.
 *
 * @param {string} str
 *   The code points of this string will be returned.
 * @returns {array}
 *   Array of code points in the string, where each is a hexidecimal string.
 */
function codePoints(str) {
  return str.split("").map(s => s.charCodeAt(0).toString(16));
}
