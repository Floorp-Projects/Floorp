/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This test checks "@" search engine aliases ("token aliases") in the urlbar.

"use strict";

const ALIAS = "@test";

add_task(async function init() {
  Services.search.addEngineWithDetails("Test", {
    alias: ALIAS,
    template: "http://example.com/?search={searchTerms}",
  });
  registerCleanupFunction(async function() {
    let engine = Services.search.getEngineByName("Test");
    Services.search.removeEngine(engine);
    // Make sure the popup is closed for the next test.
    gURLBar.handleRevert();
    gURLBar.blur();
    Assert.ok(!gURLBar.popup.popupOpen, "popup should be closed");
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
  await promiseSearchComplete();
  await waitForAutocompleteResultAt(0);
  assertAlias(true);

  EventUtils.synthesizeKey("KEY_Escape");
  await promisePopupHidden(gURLBar.popup);
});


// An alias in the middle of a string should not be recognized or highlighted.
add_task(async function charsBeforeAlias() {
  gURLBar.search("not an alias " + ALIAS);
  await promiseSearchComplete();
  await waitForAutocompleteResultAt(0);
  assertAlias(false);

  EventUtils.synthesizeKey("KEY_Escape");
  await promisePopupHidden(gURLBar.popup);
});


// Aliases are case insensitive, and the alias in the result uses the case that
// the user typed in the input.  Make sure that searching with an alias using a
// weird case still causes the alias to be highlighted.
add_task(async function aliasCase() {
  let alias = "@TeSt";
  gURLBar.search(alias);
  await promiseSearchComplete();
  await waitForAutocompleteResultAt(0);
  assertAlias(true, alias);

  EventUtils.synthesizeKey("KEY_Escape");
  await promisePopupHidden(gURLBar.popup);
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
  await promiseSearchComplete();
  await waitForAutocompleteResultAt(0);
  assertAlias(true);

  // Hide the popup.
  EventUtils.synthesizeKey("KEY_Escape");
  await promisePopupHidden(gURLBar.popup);

  // Manually set the urlbar value to a string that contains the alias at the
  // beginning but is not the same as the search string.
  let value = `${ALIAS} xxx`;
  gURLBar.value = `${ALIAS} xxx`;

  // The alias substring should not be highlighted.
  Assert.equal(gURLBar.value, value);
  Assert.ok(gURLBar.value.includes(ALIAS));
  assertHighlighted(false, ALIAS);

  // Do another search using the alias.
  searchString = `${ALIAS} bbb`;
  gURLBar.search(searchString);
  await promiseSearchComplete();
  await waitForAutocompleteResultAt(0);
  assertAlias(true);

  // Hide the popup.
  EventUtils.synthesizeKey("KEY_Escape");
  await promisePopupHidden(gURLBar.popup);

  // Manually set the urlbar value to a string that contains the alias, but not
  // at the beginning and is not the same as the search string.
  value = `bbb ${ALIAS}`;
  gURLBar.value = `bbb ${ALIAS}`;

  // The alias substring should not be highlighted.
  Assert.equal(gURLBar.value, value);
  Assert.ok(gURLBar.value.includes(ALIAS));
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
  for (let engine of Services.search.getEngines()) {
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
  info("Got token alias engines: " +
       tokenEngines.map(({ engine }) => engine.name));

  // Populate the results with the list of token alias engines by searching for
  // "@".
  gURLBar.search("@");
  await promiseSearchComplete();
  await waitForAutocompleteResultAt(tokenEngines.length - 1);

  // Key down to select each result in turn.  The urlbar value should be set to
  // each alias, and each should be highlighted.
  for (let { tokenAliases } of tokenEngines) {
    let alias = tokenAliases[0];
    EventUtils.synthesizeKey("KEY_ArrowDown");
    assertHighlighted(true, alias);
  }

  // Hide the popup.
  EventUtils.synthesizeKey("KEY_Escape");
  await promisePopupHidden(gURLBar.popup);
});


// Clicking on an @ alias in the popup should fill it in the urlbar input.
add_task(async function clickAndFillAlias() {
  // Do a search for "@" to show all the @ aliases.
  gURLBar.search("@");
  await promiseSearchComplete();

  // Find our test engine in the results.  It's probably last, but for test
  // robustness don't assume it is.
  let testEngineItem;
  for (let i = 0; !testEngineItem; i++) {
    let item = await waitForAutocompleteResultAt(i);
    let action = PlacesUtils.parseActionUrl(item.getAttribute("url"));
    if (action && action.params.alias == ALIAS) {
      testEngineItem = item;
    }
  }

  // Click it.
  let hiddenPromise = promisePopupHidden(gURLBar.popup);
  EventUtils.synthesizeMouseAtCenter(testEngineItem, {});

  // The popup will close and then open again with the new search string, which
  // should be the test alias.
  await hiddenPromise;
  await promiseSearchComplete();
  await promisePopupShown(gURLBar.popup);
  assertAlias(true);

  // The urlbar input value should be the alias followed by a space so that it's
  // ready for the user to start typing.
  Assert.equal(gURLBar.textValue, `${ALIAS} `);

  EventUtils.synthesizeKey("KEY_Escape");
  await promisePopupHidden(gURLBar.popup);
});


async function doSimpleTest(revertBetweenSteps) {
  // When autofill is enabled, searching for "@tes" will autofill to "@test",
  // which gets in the way of this test task, so temporarily disable it.
  Services.prefs.setBoolPref("browser.urlbar.autoFill", false);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.urlbar.autoFill");
  });

  // "@tes" -- not an alias, no highlight
  gURLBar.search(ALIAS.substr(0, ALIAS.length - 1));
  await promiseSearchComplete();
  await waitForAutocompleteResultAt(0);
  assertAlias(false);

  if (revertBetweenSteps) {
    gURLBar.handleRevert();
    gURLBar.blur();
  }

  // "@test" -- alias, highlight
  gURLBar.search(ALIAS);
  await promiseSearchComplete();
  await waitForAutocompleteResultAt(0);
  assertAlias(true);

  if (revertBetweenSteps) {
    gURLBar.handleRevert();
    gURLBar.blur();
  }

  // "@test foo" -- alias, highlight
  gURLBar.search(ALIAS + " foo");
  await promiseSearchComplete();
  await waitForAutocompleteResultAt(0);
  assertAlias(true);

  if (revertBetweenSteps) {
    gURLBar.handleRevert();
    gURLBar.blur();
  }

  // "@test" -- alias, highlight
  gURLBar.search(ALIAS);
  await promiseSearchComplete();
  await waitForAutocompleteResultAt(0);
  assertAlias(true);

  if (revertBetweenSteps) {
    gURLBar.handleRevert();
    gURLBar.blur();
  }

  // "@tes" -- not an alias, no highlight
  gURLBar.search(ALIAS.substr(0, ALIAS.length - 1));
  await promiseSearchComplete();
  await waitForAutocompleteResultAt(0);
  assertAlias(false);

  EventUtils.synthesizeKey("KEY_Escape");
  await promisePopupHidden(gURLBar.popup);

  Services.prefs.clearUserPref("browser.urlbar.autoFill");
}

function assertAlias(aliasPresent, expectedAlias = ALIAS) {
  assertFirstResultIsAlias(aliasPresent, expectedAlias);
  assertHighlighted(aliasPresent, expectedAlias);
}

function assertFirstResultIsAlias(isAlias, expectedAlias) {
  let controller = gURLBar.popup.input.controller;
  let action = PlacesUtils.parseActionUrl(controller.getFinalCompleteValueAt(0));
  Assert.ok(action);
  Assert.equal(action.type, "searchengine");
  Assert.equal("alias" in action.params, isAlias);
  if (isAlias) {
    Assert.equal(action.params.alias, expectedAlias);
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
  let index = gURLBar.textValue.indexOf(expectedAlias);
  Assert.ok(
    index >= 0,
    `gURLBar.textValue="${gURLBar.textValue}" expectedAlias="${expectedAlias}"`
  );
  let range = selection.getRangeAt(0);
  Assert.ok(range);
  Assert.equal(range.startOffset, index);
  Assert.equal(range.endOffset, index + expectedAlias.length);
}
