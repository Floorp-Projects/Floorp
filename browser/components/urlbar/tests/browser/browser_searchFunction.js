/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This test checks the urlbar.search() function.

"use strict";

const ALIAS = "@enginealias";
let aliasEngine;

add_task(async function init() {
  // Run this in a new tab, to ensure all the locationchange notifications have
  // fired.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  await SearchTestUtils.installSearchExtension({
    keyword: ALIAS,
  });
  aliasEngine = Services.search.getEngineByName("Example");

  registerCleanupFunction(async function() {
    BrowserTestUtils.removeTab(tab);
    gURLBar.handleRevert();
  });
});

// Calls search() with a normal, non-"@engine" search-string argument.
add_task(async function basic() {
  gURLBar.blur();
  gURLBar.search("basic");
  ok(gURLBar.hasAttribute("focused"), "url bar is focused");
  await assertUrlbarValue("basic");

  assertOneOffButtonsVisible(true);

  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );
});

// Calls search() with an invalid "@engine" search engine alias so that the
// one-off search buttons are disabled.
add_task(async function searchEngineAlias() {
  gURLBar.blur();
  await UrlbarTestUtils.promisePopupOpen(window, () =>
    gURLBar.search("@example")
  );
  ok(gURLBar.hasAttribute("focused"), "url bar is focused");
  UrlbarTestUtils.assertSearchMode(window, null);
  await assertUrlbarValue("@example");

  assertOneOffButtonsVisible(false);

  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );

  // Open the popup again (by doing another search) to make sure the one-off
  // buttons are shown -- i.e., that we didn't accidentally break them.
  await UrlbarTestUtils.promisePopupOpen(window, () =>
    gURLBar.search("not an engine alias")
  );
  await assertUrlbarValue("not an engine alias");
  assertOneOffButtonsVisible(true);

  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );
});

add_task(async function searchRestriction() {
  gURLBar.blur();
  await UrlbarTestUtils.promisePopupOpen(window, () =>
    gURLBar.search(UrlbarTokenizer.RESTRICT.SEARCH)
  );
  ok(gURLBar.hasAttribute("focused"), "url bar is focused");
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: UrlbarSearchUtils.getDefaultEngine().name,
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    // Entry is "other" because we didn't pass searchModeEntry to search().
    entry: "other",
  });
  assertOneOffButtonsVisible(true);
  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window);
});

add_task(async function historyRestriction() {
  gURLBar.blur();
  await UrlbarTestUtils.promisePopupOpen(window, () =>
    gURLBar.search(UrlbarTokenizer.RESTRICT.HISTORY)
  );
  ok(gURLBar.hasAttribute("focused"), "url bar is focused");
  await UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.HISTORY,
    entry: "other",
  });
  assertOneOffButtonsVisible(true);
  Assert.ok(!gURLBar.value, "The Urlbar has no value.");
  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window);
});

add_task(async function historyRestrictionWithString() {
  gURLBar.blur();
  // The leading and trailing spaces are intentional to verify that search()
  // preserves them.
  let searchString = " foo bar ";
  await UrlbarTestUtils.promisePopupOpen(window, () =>
    gURLBar.search(`${UrlbarTokenizer.RESTRICT.HISTORY} ${searchString}`)
  );
  ok(gURLBar.hasAttribute("focused"), "url bar is focused");
  await UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.HISTORY,
    entry: "other",
  });
  // We don't use assertUrlbarValue here since we expect to open a local search
  // mode. In those modes, we don't show a heuristic search result, which
  // assertUrlbarValue checks for.
  await UrlbarTestUtils.promiseSearchComplete(window);
  Assert.equal(
    gURLBar.value,
    searchString,
    "The Urlbar value should be the search string."
  );
  assertOneOffButtonsVisible(true);
  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window);
});

add_task(async function tagRestriction() {
  gURLBar.blur();
  await UrlbarTestUtils.promisePopupOpen(window, () =>
    gURLBar.search(UrlbarTokenizer.RESTRICT.TAG)
  );
  ok(gURLBar.hasAttribute("focused"), "url bar is focused");
  // Since tags are not a supported search mode, we should just insert the tag
  // restriction token and not enter search mode.
  await UrlbarTestUtils.assertSearchMode(window, null);
  await assertUrlbarValue(`${UrlbarTokenizer.RESTRICT.TAG} `);
  assertOneOffButtonsVisible(true);
  await UrlbarTestUtils.promisePopupClose(window);
});

// Calls search() twice with the same value. The popup should reopen.
add_task(async function searchTwice() {
  gURLBar.blur();
  await UrlbarTestUtils.promisePopupOpen(window, () => gURLBar.search("test"));
  ok(gURLBar.hasAttribute("focused"), "url bar is focused");
  await assertUrlbarValue("test");
  assertOneOffButtonsVisible(true);
  await UrlbarTestUtils.promisePopupClose(window);

  await UrlbarTestUtils.promisePopupOpen(window, () => gURLBar.search("test"));
  ok(gURLBar.hasAttribute("focused"), "url bar is focused");
  await assertUrlbarValue("test");
  assertOneOffButtonsVisible(true);
  await UrlbarTestUtils.promisePopupClose(window);
});

// Calls search() during an IME composition.
add_task(async function searchIME() {
  // First run a search.
  gURLBar.blur();
  await UrlbarTestUtils.promisePopupOpen(window, () => gURLBar.search("test"));
  ok(gURLBar.hasAttribute("focused"), "url bar is focused");
  await assertUrlbarValue("test");
  // Start composition.
  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeComposition({ type: "compositionstart" })
  );

  gURLBar.search("test");
  // Unfortunately there's no other way to check we don't open the view than to
  // wait for it.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 1000));
  ok(!UrlbarTestUtils.isPopupOpen(window), "The panel should still be closed");

  await UrlbarTestUtils.promisePopupOpen(window, () =>
    EventUtils.synthesizeComposition({ type: "compositioncommitasis" })
  );

  assertOneOffButtonsVisible(true);

  await UrlbarTestUtils.promisePopupClose(window);
});

// Calls search() with an engine alias.
add_task(async function searchWithAlias() {
  await UrlbarTestUtils.promisePopupOpen(window, async () =>
    gURLBar.search(`${ALIAS} test`, {
      searchEngine: aliasEngine,
      searchModeEntry: "handoff",
    })
  );
  Assert.ok(gURLBar.hasAttribute("focused"), "Urlbar is focused");

  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: aliasEngine.name,
    entry: "handoff",
  });
  await assertUrlbarValue("test");
  assertOneOffButtonsVisible(true);
  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window);
});

/**
 * Asserts that the one-off search buttons are or aren't visible.
 *
 * @param {boolean} visible
 *        True if they should be visible, false if not.
 */
function assertOneOffButtonsVisible(visible) {
  Assert.equal(
    UrlbarTestUtils.getOneOffSearchButtonsVisible(window),
    visible,
    "Should show or not the one-off search buttons"
  );
}

/**
 * Asserts that the urlbar's input value is the given value.  Also asserts that
 * the first (heuristic) result in the popup is a search suggestion whose search
 * query is the given value.
 *
 * @param {string} value
 *        The urlbar's expected value.
 */
async function assertUrlbarValue(value) {
  await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);

  Assert.equal(gURLBar.value, value);
  Assert.greater(
    UrlbarTestUtils.getResultCount(window),
    0,
    "Should have at least one result"
  );

  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(
    result.type,
    UrlbarUtils.RESULT_TYPE.SEARCH,
    "Should have type search for the first result"
  );
  // Strip search restriction token from value.
  if (value[0] == UrlbarTokenizer.RESTRICT.SEARCH) {
    value = value.substring(1).trim();
  }
  Assert.equal(
    result.searchParams.query,
    value,
    "Should have the correct query for the first result"
  );
}
