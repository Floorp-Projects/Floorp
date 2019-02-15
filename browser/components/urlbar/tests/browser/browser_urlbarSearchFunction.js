/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This test checks the urlbar.search() function.

"use strict";

add_task(async function init() {
  // Run this in a new tab, to ensure all the locationchange notifications have
  // fired.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  let which = gURLBar._whichSearchSuggestionsNotification || undefined;
  registerCleanupFunction(async function() {
    BrowserTestUtils.removeTab(tab);
    // Reset the search suggestions notification.
    if (which === undefined) {
      delete gURLBar._whichSearchSuggestionsNotification;
    } else {
      gURLBar._whichSearchSuggestionsNotification = which;
    }
    Services.prefs.clearUserPref("timesBeforeHidingSuggestionsHint");

    gURLBar.handleRevert();
    await UrlbarTestUtils.promisePopupClose(window);
  });
});


// Calls search() with a normal, non-"@engine" search-string argument.
add_task(async function basic() {
  let resetNotification = enableSearchSuggestionsNotification();

  gURLBar.search("basic");
  await assertUrlbarValue("basic");

  assertSearchSuggestionsNotificationVisible(true);
  assertOneOffButtonsVisible(true);

  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape"));

  resetNotification();
});


// Calls search() with an "@engine" search engine alias so that the one-off
// search buttons and search-suggestions notification are disabled.
add_task(async function searchEngineAlias() {
  let resetNotification = enableSearchSuggestionsNotification();

  gURLBar.search("@example");
  await assertUrlbarValue("@example");

  assertSearchSuggestionsNotificationVisible(false);
  assertOneOffButtonsVisible(false);

  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape"));

  // Open the popup again (by doing another search) to make sure the
  // notification and one-off buttons are shown -- i.e., that we didn't
  // accidentally break them.
  gURLBar.search("not an engine alias");
  await assertUrlbarValue("not an engine alias");
  assertSearchSuggestionsNotificationVisible(true);
  assertOneOffButtonsVisible(true);

  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape"));

  resetNotification();
});


/**
 * Makes sure the search-suggestions notification will be shown the next several
 * times the popup opens.
 *
 * @returns {function} A function that you should call when you're done that
 *   resets the state state of the notification.
 */
function enableSearchSuggestionsNotification() {
  let which = gURLBar._whichSearchSuggestionsNotification || undefined;
  gURLBar._whichSearchSuggestionsNotification = "opt-out";
  Services.prefs.setIntPref("timesBeforeHidingSuggestionsHint", 10);
  return function reset() {
    if (which === undefined) {
      delete gURLBar._whichSearchSuggestionsNotification;
    } else {
      gURLBar._whichSearchSuggestionsNotification = which;
    }
    Services.prefs.clearUserPref("timesBeforeHidingSuggestionsHint");
  };
}

/**
 * Asserts that the search-suggestion notification is or isn't visible.
 *
 * @param {boolean} visible
 *        True if it should be visible, false if not.
 */
function assertSearchSuggestionsNotificationVisible(visible) {
  // TODO Bug 1525296: Not implemented for QuantumBar.
  if (UrlbarPrefs.get("quantumbar")) {
    return;
  }
  Assert.equal(
    gURLBar.popup.classList.contains("showSearchSuggestionsNotification"),
    visible
  );
  Assert.equal(
    window.getComputedStyle(gURLBar.popup.searchSuggestionsNotification).display,
    visible ? "-moz-deck" : "none"
  );
}

/**
 * Asserts that the one-off search buttons are or aren't visible.
 *
 * @param {boolean} visible
 *        True if they should be visible, false if not.
 */
function assertOneOffButtonsVisible(visible) {
  // TODO Bug 1527934: Not implemented for QuantumBar.
  if (UrlbarPrefs.get("quantumbar")) {
    return;
  }
  Assert.equal(UrlbarTestUtils.getOneOffSearchButtonsVisible(window), visible,
    "Should show or not the one-off search buttons");
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
  await waitForAutocompleteResultAt(0);

  Assert.equal(gURLBar.value, value);
  Assert.greater(UrlbarTestUtils.getResultCount(window), 0,
    "Should have at least one result");

  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.SEARCH,
    "Should have type search for the first result");
  Assert.equal(result.searchParams.query, value,
    "Should have the correct query for the first result");
}
