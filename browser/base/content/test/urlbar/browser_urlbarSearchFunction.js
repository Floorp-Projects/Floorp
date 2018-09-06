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

    // Make sure the popup is closed for the next test.
    gURLBar.handleRevert();
    gURLBar.blur();
    Assert.ok(!gURLBar.popup.popupOpen, "popup should be closed");
  });
});


// Calls search() with only a search-string argument.
add_task(async function basic() {
  let resetNotification = enableSearchSuggestionsNotification();

  gURLBar.search("basic");
  await promiseSearchComplete();
  await waitForAutocompleteResultAt(0);
  assertUrlbarValue("basic");

  assertSearchSuggestionsNotificationVisible(true);
  assertOneOffButtonsVisible(true);

  EventUtils.synthesizeKey("KEY_Escape");
  await promisePopupHidden(gURLBar.popup);

  resetNotification();
});


// Calls search() with the search-suggestions notification disabled.
add_task(async function disableSearchSuggestionsNotification() {
  let resetNotification = enableSearchSuggestionsNotification();

  gURLBar.search("disableSearchSuggestionsNotification", {
    disableSearchSuggestionsNotification: true,
  });
  await promiseSearchComplete();
  await waitForAutocompleteResultAt(0);
  assertUrlbarValue("disableSearchSuggestionsNotification");

  assertSearchSuggestionsNotificationVisible(false);
  assertOneOffButtonsVisible(true);

  EventUtils.synthesizeKey("KEY_Escape");
  await promisePopupHidden(gURLBar.popup);

  // Open the popup again (by doing another search) to make sure the
  // notification is shown -- i.e., that we didn't accidentally break it if
  // it should continue being shown.
  gURLBar.search("disableSearchSuggestionsNotification again");
  await promiseSearchComplete();
  await waitForAutocompleteResultAt(0);
  assertUrlbarValue("disableSearchSuggestionsNotification again");
  assertSearchSuggestionsNotificationVisible(true);
  assertOneOffButtonsVisible(true);

  EventUtils.synthesizeKey("KEY_Escape");
  await promisePopupHidden(gURLBar.popup);

  resetNotification();
});


// Calls search() with the one-off search buttons disabled.
add_task(async function disableOneOffButtons() {
  let resetNotification = enableSearchSuggestionsNotification();

  gURLBar.search("disableOneOffButtons", {
    disableOneOffButtons: true,
  });
  await promiseSearchComplete();
  await waitForAutocompleteResultAt(0);
  assertUrlbarValue("disableOneOffButtons");

  assertSearchSuggestionsNotificationVisible(true);
  assertOneOffButtonsVisible(false);

  EventUtils.synthesizeKey("KEY_Escape");
  await promisePopupHidden(gURLBar.popup);

  // Open the popup again (by doing another search) to make sure the one-off
  // buttons are shown -- i.e., that we didn't accidentally break them.
  gURLBar.search("disableOneOffButtons again");
  await promiseSearchComplete();
  await waitForAutocompleteResultAt(0);
  assertUrlbarValue("disableOneOffButtons again");
  assertSearchSuggestionsNotificationVisible(true);
  assertOneOffButtonsVisible(true);

  EventUtils.synthesizeKey("KEY_Escape");
  await promisePopupHidden(gURLBar.popup);

  resetNotification();
});


// Calls search() with both the search-suggestions notification and the one-off
// search buttons disabled.
add_task(async function disableSearchSuggestionsNotificationAndOneOffButtons() {
  let resetNotification = enableSearchSuggestionsNotification();

  gURLBar.search("disableSearchSuggestionsNotificationAndOneOffButtons", {
    disableSearchSuggestionsNotification: true,
    disableOneOffButtons: true,
  });
  await promiseSearchComplete();
  await waitForAutocompleteResultAt(0);
  assertUrlbarValue("disableSearchSuggestionsNotificationAndOneOffButtons");

  assertSearchSuggestionsNotificationVisible(false);
  assertOneOffButtonsVisible(false);

  EventUtils.synthesizeKey("KEY_Escape");
  await promisePopupHidden(gURLBar.popup);

  // Open the popup again (by doing another search) to make sure the
  // notification and one-off buttons are shown -- i.e., that we didn't
  // accidentally break them.
  gURLBar.search("disableSearchSuggestionsNotificationAndOneOffButtons again");
  await promiseSearchComplete();
  await waitForAutocompleteResultAt(0);
  assertUrlbarValue("disableSearchSuggestionsNotificationAndOneOffButtons again");
  assertSearchSuggestionsNotificationVisible(true);
  assertOneOffButtonsVisible(true);

  EventUtils.synthesizeKey("KEY_Escape");
  await promisePopupHidden(gURLBar.popup);

  resetNotification();
});


/**
 * Makes sure the search-suggestions notification will be shown the next several
 * times the popup opens.
 *
 * @return  A function that you should call when you're done that resets the
 *          state of the notification.
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
 * @param visible
 *        True if it should be visible, false if not.
 */
function assertSearchSuggestionsNotificationVisible(visible) {
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
 * @param visible
 *        True if they should be visible, false if not.
 */
function assertOneOffButtonsVisible(visible) {
  Assert.equal(gURLBar.popup.oneOffSearchesEnabled, visible);
  Assert.equal(
    window.getComputedStyle(gURLBar.popup.oneOffSearchButtons).display,
    visible ? "-moz-box" : "none"
  );
}

/**
 * Asserts that the urlbar's input value is the given value.  Also asserts that
 * the first (heuristic) result in the popup is a search suggestion whose search
 * query is the given value.
 *
 * @param value
 *        The urlbar's expected value.
 */
function assertUrlbarValue(value) {
  Assert.equal(gURLBar.value, value);
  let controller = gURLBar.controller;
  Assert.ok(controller.matchCount > 0);
  let action = gURLBar._parseActionUrl(controller.getValueAt(0));
  Assert.ok(action);
  Assert.equal(action.type, "searchengine");
  Assert.equal(action.params.searchQuery, value);
}
