/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This test makes sure that the up/down, page-up/down, and tab keys properly
// adjust the selection.  See also browser_caret_navigation.js.

"use strict";

const MAX_RESULTS = UrlbarPrefs.get("maxRichResults");

add_task(async function init() {
  for (let i = 0; i < MAX_RESULTS; i++) {
    await PlacesTestUtils.addVisits("http://example.com/" + i);
  }
  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
  });
});

add_task(async function downKey() {
  await promiseAutocompleteResultPopup("exam", window, true);
  Assert.equal(UrlbarTestUtils.getSelectedIndex(window), 0,
               "The heuristic autofill result should be selected initially");
  for (let i = 1; i < MAX_RESULTS; i++) {
    EventUtils.synthesizeKey("KEY_ArrowDown");
    Assert.equal(UrlbarTestUtils.getSelectedIndex(window), i);
  }
  EventUtils.synthesizeKey("KEY_ArrowDown");
  let oneOffs = UrlbarTestUtils.getOneOffSearchButtons(window);
  Assert.ok(oneOffs.selectedButton, "A one-off should now be selected");
  while (oneOffs.selectedButton) {
    EventUtils.synthesizeKey("KEY_ArrowDown");
  }
  Assert.equal(UrlbarTestUtils.getSelectedIndex(window), 0,
               "The heuristic autofill result should be selected again");
});

add_task(async function upKey() {
  await promiseAutocompleteResultPopup("exam", window, true);
  Assert.equal(UrlbarTestUtils.getSelectedIndex(window), 0,
               "The heuristic autofill result should be selected initially");
  EventUtils.synthesizeKey("KEY_ArrowUp");
  let oneOffs = UrlbarTestUtils.getOneOffSearchButtons(window);
  Assert.ok(oneOffs.selectedButton, "A one-off should now be selected");
  while (oneOffs.selectedButton) {
    EventUtils.synthesizeKey("KEY_ArrowUp");
  }
  Assert.equal(UrlbarTestUtils.getSelectedIndex(window),
               MAX_RESULTS - 1,
               "The last result should be selected");
  for (let i = 1; i < MAX_RESULTS; i++) {
    EventUtils.synthesizeKey("KEY_ArrowUp");
    Assert.equal(UrlbarTestUtils.getSelectedIndex(window),
                 MAX_RESULTS - i - 1);
  }
});

add_task(async function pageDownKey() {
  await promiseAutocompleteResultPopup("exam", window, true);
  Assert.equal(UrlbarTestUtils.getSelectedIndex(window), 0,
               "The heuristic autofill result should be selected initially");
  let pageCount = Math.ceil((MAX_RESULTS - 1) / UrlbarUtils.PAGE_UP_DOWN_DELTA);
  for (let i = 0; i < pageCount; i++) {
    EventUtils.synthesizeKey("KEY_PageDown");
    Assert.equal(
      UrlbarTestUtils.getSelectedIndex(window),
      Math.min((i + 1) * UrlbarUtils.PAGE_UP_DOWN_DELTA, MAX_RESULTS - 1)
    );
  }
  EventUtils.synthesizeKey("KEY_PageDown");
  Assert.equal(UrlbarTestUtils.getSelectedIndex(window), 0,
               "Page down at end should wrap around to first result");
});

add_task(async function pageUpKey() {
  await promiseAutocompleteResultPopup("exam", window, true);
  Assert.equal(UrlbarTestUtils.getSelectedIndex(window), 0,
               "The heuristic autofill result should be selected initially");
  EventUtils.synthesizeKey("KEY_PageUp");
  Assert.equal(UrlbarTestUtils.getSelectedIndex(window), MAX_RESULTS - 1,
               "Page up at start should wrap around to last result");
  let pageCount = Math.ceil((MAX_RESULTS - 1) / UrlbarUtils.PAGE_UP_DOWN_DELTA);
  for (let i = 0; i < pageCount; i++) {
    EventUtils.synthesizeKey("KEY_PageUp");
    Assert.equal(
      UrlbarTestUtils.getSelectedIndex(window),
      Math.max(MAX_RESULTS - 1 - ((i + 1) * UrlbarUtils.PAGE_UP_DOWN_DELTA), 0)
    );
  }
});

add_task(async function pageDownKeyShowsView() {
  await promiseAutocompleteResultPopup("exam", window, true);
  await UrlbarTestUtils.promisePopupClose(window);
  EventUtils.synthesizeKey("KEY_PageDown");
  await UrlbarTestUtils.promiseSearchComplete(window);
  Assert.ok(UrlbarTestUtils.isPopupOpen(window));
  Assert.equal(UrlbarTestUtils.getSelectedIndex(window), 0);
});

add_task(async function pageUpKeyShowsView() {
  await promiseAutocompleteResultPopup("exam", window, true);
  await UrlbarTestUtils.promisePopupClose(window);
  EventUtils.synthesizeKey("KEY_PageUp");
  await UrlbarTestUtils.promiseSearchComplete(window);
  Assert.ok(UrlbarTestUtils.isPopupOpen(window));
  Assert.equal(UrlbarTestUtils.getSelectedIndex(window), 0);
});

add_task(async function tabKey() {
  await promiseAutocompleteResultPopup("exam", window, true);
  Assert.equal(UrlbarTestUtils.getSelectedIndex(window), 0,
               "The heuristic autofill result should be selected initially");
  for (let i = 1; i < MAX_RESULTS; i++) {
    EventUtils.synthesizeKey("KEY_Tab");
    Assert.equal(UrlbarTestUtils.getSelectedIndex(window), i);
  }
  EventUtils.synthesizeKey("KEY_Tab");
  Assert.equal(UrlbarTestUtils.getSelectedIndex(window), 0,
               "The heuristic autofill result should be selected again");
});

add_task(async function tabKeyReverse() {
  await promiseAutocompleteResultPopup("exam", window, true);
  Assert.equal(UrlbarTestUtils.getSelectedIndex(window), 0,
               "The heuristic autofill result should be selected initially");
  for (let i = 1; i < MAX_RESULTS; i++) {
    EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
    Assert.equal(UrlbarTestUtils.getSelectedIndex(window), MAX_RESULTS - i);
  }
});

add_task(async function tabKeyBlur() {
  await promiseAutocompleteResultPopup("exam", window, true);
  await UrlbarTestUtils.promisePopupClose(window);
  Assert.equal(document.activeElement, gURLBar.inputField);
  EventUtils.synthesizeKey("KEY_Tab");
  Assert.notEqual(document.activeElement, gURLBar.inputField);
});
