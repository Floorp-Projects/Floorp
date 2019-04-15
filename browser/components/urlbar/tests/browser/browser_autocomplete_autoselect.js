/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the first item is correctly autoselected and some navigation
 * around the results list.
 */

const ONEOFF_URLBAR_PREF = "browser.urlbar.oneOffSearches";

function repeat(limit, func) {
  for (let i = 0; i < limit; i++) {
    func(i);
  }
}

function assertSelected(index) {
  Assert.equal(UrlbarTestUtils.getSelectedIndex(window),
    index, "Should have selected the correct item");
  // Also check the "selected" attribute, to ensure it is not a "fake" selection
  // due to binding misbehaviors.
  let element = UrlbarTestUtils.getSelectedElement(window);
  Assert.ok(element.hasAttribute("selected"),
    "Should have the selected attribute on the row element");

  // This is true because although both the listbox and the one-offs can have
  // selections, the test doesn't check that.
  Assert.equal(UrlbarTestUtils.getOneOffSearchButtons(window).selectedButton, null,
     "A result is selected, so the one-offs should not have a selection");
}

function assertSelected_one_off(index) {
  Assert.equal(UrlbarTestUtils.getOneOffSearchButtons(window).selectedButtonIndex, index,
     "Expected one-off button should be selected");

  // This is true because although both the listbox and the one-offs can have
  // selections, the test doesn't check that.
  Assert.equal(UrlbarTestUtils.getSelectedIndex(window), -1,
     "A one-off is selected, so the listbox should not have a selection");
}

add_task(async function() {
  let maxResults = Services.prefs.getIntPref("browser.urlbar.maxRichResults");
  Services.prefs.setBoolPref(ONEOFF_URLBAR_PREF, true);
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:mozilla");
  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
    Services.prefs.clearUserPref(ONEOFF_URLBAR_PREF);
    BrowserTestUtils.removeTab(tab);
  });

  let visits = [];
  repeat(maxResults, i => {
    visits.push({
      uri: makeURI("http://example.com/autocomplete/?" + i),
    });
  });
  await PlacesTestUtils.addVisits(visits);

  await promiseAutocompleteResultPopup("example.com/autocomplete", window, true);

  let resultCount = await UrlbarTestUtils.getResultCount(window);

  Assert.equal(resultCount, maxResults,
    "Should get the expected amount of results");
  assertSelected(0);

  info("Key Down to select the next item");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  assertSelected(1);

  info("Key Down maxResults-1 times should select the first one-off");
  repeat(maxResults - 1, () => EventUtils.synthesizeKey("KEY_ArrowDown"));
  assertSelected_one_off(0);

  info("Key Down numButtons-1 should select the last one-off");
  let numButtons = UrlbarTestUtils.getOneOffSearchButtons(window)
    .getSelectableButtons(true).length;
  repeat(numButtons - 1, () => EventUtils.synthesizeKey("KEY_ArrowDown"));
  assertSelected_one_off(numButtons - 1);

  info("Key Down twice more should select the second result");
  repeat(2, () => EventUtils.synthesizeKey("KEY_ArrowDown"));
  assertSelected(1);

  info("Key Down maxResults + numButtons times should wrap around");
  repeat(maxResults + numButtons,
         () => EventUtils.synthesizeKey("KEY_ArrowDown"));
  assertSelected(1);

  info("Key Up maxResults + numButtons times should wrap around the other way");
  repeat(maxResults + numButtons, () => EventUtils.synthesizeKey("KEY_ArrowUp"));
  assertSelected(1);

  info("Page Up will go up the list, but not wrap");
  EventUtils.synthesizeKey("KEY_PageUp");
  assertSelected(0);

  info("Page Up again will wrap around to the end of the list");
  EventUtils.synthesizeKey("KEY_PageUp");
  assertSelected(maxResults - 1);
});
