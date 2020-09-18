/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that user-defined aliases are replaced by the search mode indicator.
 */

const ALIAS = "testalias";
const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

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

  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.update2", true]],
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
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: ALIAS + " ",
  });
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: aliasEngine.name,
    entry: "typed",
  });
  Assert.ok(!gURLBar.value, "The urlbar value should be cleared.");
  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window);
});

// A complete alias should be replaced after typing a space.
add_task(async function trailingSpace_typed() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: ALIAS,
  });
  await UrlbarTestUtils.assertSearchMode(window, null);

  // We need to wait for two searches: The first enters search mode, the second
  // does the search in search mode.
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeKey(" ");
  await searchPromise;

  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: aliasEngine.name,
    entry: "typed",
  });
  Assert.ok(!gURLBar.value, "The urlbar value should be cleared.");
  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window);
});

// A complete alias with a trailing space should be replaced, and the query
// after the trailing space should be the new value of the input.
add_task(async function trailingSpace_query() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: ALIAS + " query",
  });

  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: aliasEngine.name,
    entry: "typed",
  });
  Assert.equal(gURLBar.value, "query", "The urlbar value should be the query.");
  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window);
});
