/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that user-defined aliases are replaced by the search mode indicator.
 */

const ALIAS = "testalias";
const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

// We make sure that aliases and search terms are correctly recognized when they
// are separated by each of these different types of spaces and combinations of
// spaces.  U+3000 is the ideographic space in CJK and is commonly used by CJK
// speakers.
const TEST_SPACES = [" ", "\u3000", " \u3000", "\u3000 "];

let defaultEngine, aliasEngine;

add_task(async function setup() {
  defaultEngine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME
  );
  defaultEngine.alias = "@default";
  let oldDefaultEngine = await Services.search.getDefault();
  Services.search.setDefault(defaultEngine);
  aliasEngine = await Services.search.addEngineWithDetails("Test", {
    alias: ALIAS,
    template: "http://example.com/?search={searchTerms}",
  });

  registerCleanupFunction(async function() {
    await Services.search.removeEngine(aliasEngine);
    Services.search.setDefault(oldDefaultEngine);
  });
});

// An incomplete alias should not be replaced.
add_task(async function incompleteAlias() {
  // Check that a non-fully typed alias is not replaced.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: ALIAS.slice(0, -1),
  });
  await UrlbarTestUtils.assertSearchMode(window, null);

  // Type a space just to make sure it's not replaced.
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeKey(" ");
  await searchPromise;

  await UrlbarTestUtils.assertSearchMode(window, null);
  Assert.equal(
    gURLBar.value,
    ALIAS.slice(0, -1) + " ",
    "The typed value should be unchanged except for the space."
  );
  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );
});

// A complete alias without a trailing space should not be replaced.
add_task(async function noTrailingSpace() {
  let value = ALIAS;
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value,
  });
  await UrlbarTestUtils.assertSearchMode(window, null);
  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );
});

// A complete typed alias without a trailing space should not be replaced.
add_task(async function noTrailingSpace_typed() {
  // Start by searching for the alias minus its last char.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: ALIAS.slice(0, -1),
  });
  await UrlbarTestUtils.assertSearchMode(window, null);

  // Now type the last char.
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeKey(ALIAS.slice(-1));
  await searchPromise;

  await UrlbarTestUtils.assertSearchMode(window, null);
  Assert.equal(
    gURLBar.value,
    ALIAS,
    "The typed value should be the full alias."
  );

  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );
});

// A complete alias with a trailing space should be replaced.
add_task(async function trailingSpace() {
  for (let spaces of TEST_SPACES) {
    info("Testing: " + JSON.stringify({ spaces: codePoints(spaces) }));
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: ALIAS + spaces,
    });
    await UrlbarTestUtils.assertSearchMode(window, {
      engineName: aliasEngine.name,
      entry: "typed",
    });
    Assert.ok(!gURLBar.value, "The urlbar value should be cleared.");
    await UrlbarTestUtils.exitSearchMode(window);
    await UrlbarTestUtils.promisePopupClose(window);
  }
});

// A complete alias should be replaced after typing a space.
add_task(async function trailingSpace_typed() {
  for (let spaces of TEST_SPACES) {
    if (spaces.length != 1) {
      continue;
    }
    info("Testing: " + JSON.stringify({ spaces: codePoints(spaces) }));

    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: ALIAS,
    });
    await UrlbarTestUtils.assertSearchMode(window, null);

    // We need to wait for two searches: The first enters search mode, the second
    // does the search in search mode.
    let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
    EventUtils.synthesizeKey(spaces);
    await searchPromise;

    await UrlbarTestUtils.assertSearchMode(window, {
      engineName: aliasEngine.name,
      entry: "typed",
    });
    Assert.ok(!gURLBar.value, "The urlbar value should be cleared.");
    await UrlbarTestUtils.exitSearchMode(window);
    await UrlbarTestUtils.promisePopupClose(window);
  }
});

// A complete alias with a trailing space should be replaced, and the query
// after the trailing space should be the new value of the input.
add_task(async function trailingSpace_query() {
  for (let spaces of TEST_SPACES) {
    info("Testing: " + JSON.stringify({ spaces: codePoints(spaces) }));

    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: ALIAS + spaces + "query",
    });

    await UrlbarTestUtils.assertSearchMode(window, {
      engineName: aliasEngine.name,
      entry: "typed",
    });
    Assert.equal(
      gURLBar.value,
      "query",
      "The urlbar value should be the query."
    );
    await UrlbarTestUtils.exitSearchMode(window);
    await UrlbarTestUtils.promisePopupClose(window);
  }
});

add_task(async function() {
  info("Test search mode when typing an alias after selecting one-off button");

  info("Open the result popup");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });

  info("Select one of one-off button");
  const oneOffs = UrlbarTestUtils.getOneOffSearchButtons(window);
  await TestUtils.waitForCondition(
    () => !oneOffs._rebuilding,
    "Waiting for one-offs to finish rebuilding"
  );
  EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
  ok(oneOffs.selectedButton, "There is a selected one-off button");
  const selectedEngine = oneOffs.selectedButton.engine;
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: selectedEngine.name,
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    entry: "oneoff",
    isPreview: true,
  });

  info("Type a search engine alias and query");
  const inputString = "@default query";
  inputString.split("").forEach(c => EventUtils.synthesizeKey(c));
  await UrlbarTestUtils.promiseSearchComplete(window);
  Assert.equal(
    gURLBar.value,
    inputString,
    "Alias and query is inputed correctly to the urlbar"
  );
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: selectedEngine.name,
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    entry: "oneoff",
  });

  // When starting typing, as the search mode is confirmed, the one-off
  // selection is removed.
  ok(!oneOffs.selectedButton, "There is no any selected one-off button");

  // Clean up
  gURLBar.value = "";
  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window);
});

add_task(async function() {
  info(
    "Test search mode after removing current search mode when multiple aliases are written"
  );

  info("Open the result popup with multiple aliases");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "@default testalias @default",
  });

  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: defaultEngine.name,
    entry: "typed",
  });
  Assert.equal(
    gURLBar.value,
    "testalias @default",
    "The value on the urlbar is correct"
  );

  info("Exit search mode by clicking");
  const indicator = gURLBar.querySelector("#urlbar-search-mode-indicator");
  EventUtils.synthesizeMouseAtCenter(indicator, { type: "mouseover" }, window);
  const closeButton = gURLBar.querySelector(
    "#urlbar-search-mode-indicator-close"
  );
  const searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeMouseAtCenter(closeButton, {}, window);
  await searchPromise;

  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: aliasEngine.name,
    entry: "typed",
  });
  Assert.equal(gURLBar.value, "@default", "The value on the urlbar is correct");

  // Clean up
  gURLBar.value = "";
  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window);
});

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
