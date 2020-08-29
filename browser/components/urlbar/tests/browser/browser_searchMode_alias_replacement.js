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

// Tests that a fully typed alias is replaced when space is pressed.
add_task(async function replaced_on_space() {
  // Check that a non-fully typed alias is not replaced.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: ALIAS.slice(0, -1),
  });

  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeKey("VK_SPACE");
  await searchPromise;

  await UrlbarTestUtils.assertSearchMode(window, null);
  Assert.equal(
    gURLBar.value,
    ALIAS.slice(0, -1),
    "The typed value should be unchanged."
  );
  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );

  // Test that the alias is replaced when it is fully typed.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: `${ALIAS} `,
  });
  let keywordOfferResult = await UrlbarTestUtils.getDetailsOfResultAt(
    window,
    0
  );
  Assert.equal(
    keywordOfferResult.searchParams.keyword,
    ALIAS,
    "The first result should be a keyword search result with the correct alias."
  );
  Assert.equal(
    keywordOfferResult.searchParams.engine,
    aliasEngine.name,
    "The first result should be a keyword search result with the correct engine."
  );

  searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  // Fire an input event simulating typing a space after the ALIAS.
  UrlbarTestUtils.fireInputEvent(window);
  await searchPromise;

  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: aliasEngine.name,
  });
  Assert.ok(!gURLBar.value, "The Urlbar value should be cleared.");
  await UrlbarTestUtils.exitSearchMode(window, { backspace: true });
  await UrlbarTestUtils.promisePopupClose(window);
});

add_task(async function not_replaced_for_alt_tab() {
  // Test that the alias is replaced when it is fully typed.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: `${ALIAS} `,
  });
  let keywordOfferResult = await UrlbarTestUtils.getDetailsOfResultAt(
    window,
    0
  );
  Assert.equal(
    keywordOfferResult.searchParams.keyword,
    ALIAS,
    "The first result should be a keyword search result with the correct alias."
  );
  Assert.equal(
    keywordOfferResult.searchParams.engine,
    aliasEngine.name,
    "The first result should be a keyword search result with the correct engine."
  );

  EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true, repeat: 2 });
  keywordOfferResult = await UrlbarTestUtils.waitForAutocompleteResultAt(
    window,
    0
  );
  Assert.ok(
    keywordOfferResult.result.payload.originalEngine,
    "The keyword offer result now has the originalEngine property."
  );
  Assert.notEqual(
    keywordOfferResult.result.payload.engine,
    keywordOfferResult.result.payload.originalEngine,
    "engine and originalEngine are different."
  );

  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  // Fire an input event simulating typing a space after the ALIAS.
  UrlbarTestUtils.fireInputEvent(window);
  await searchPromise;

  await UrlbarTestUtils.assertSearchMode(window, null);
});
