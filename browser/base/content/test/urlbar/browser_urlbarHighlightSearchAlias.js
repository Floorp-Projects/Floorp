/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This test checks the urlbar.highlightSearchAlias() function.

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

add_task(async function testNoRevert() {
  await doTest(false);
});

add_task(async function testRevert() {
  await doTest(true);
});

add_task(async function spacesBeforeAlias() {
  gURLBar.search("     " + ALIAS);
  await promiseSearchComplete();
  await waitForAutocompleteResultAt(0);
  assertAlias(true);

  EventUtils.synthesizeKey("KEY_Escape");
  await promisePopupHidden(gURLBar.popup);
});

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
});


async function doTest(revertBetweenSteps) {
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
  let index = gURLBar.value.indexOf(expectedAlias);
  Assert.ok(index >= 0);
  let range = selection.getRangeAt(0);
  Assert.ok(range);
  Assert.equal(range.startOffset, index);
  Assert.equal(range.endOffset, index + expectedAlias.length);
}
