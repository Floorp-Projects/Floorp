/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that editing text inside parens behave as expected, i.e.
// - it does not show the autocompletion text
// - show popup when there's properties to complete
// - insert the selected item from the popup in the input
// - right arrow dismiss popup and don't autocomplete
// - tab key when there is not visible autocomplete suggestion insert a tab
// See Bug 812618, 1479521 and 1334130.

const TEST_URI = `data:text/html;charset=utf-8,
<head>
  <script>
    window.testBugAA = "hello world";
    window.testBugBB = "hello world 2";
  </script>
</head>
<body>bug 812618 - test completion inside text</body>`;

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;
  info("web console opened");

  const { autocompletePopup: popup } = jsterm;

  await setInitialState(hud);

  ok(popup.isOpen, "popup is open");
  is(popup.itemCount, 2, "popup.itemCount is correct");
  is(popup.selectedIndex, 0, "popup.selectedIndex is correct");
  ok(!getInputCompletionValue(hud), "there is no completion text");

  info("Pressing arrow right");
  let onPopupClose = popup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_ArrowRight");
  await onPopupClose;
  ok(true, "popup was closed");
  checkInputValueAndCursorPosition(
    hud,
    "dump(window.testB)|",
    "input wasn't modified"
  );

  await setInitialState(hud);
  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(popup.selectedIndex, 1, "popup.selectedIndex is correct");
  ok(!getInputCompletionValue(hud), "completeNode.value is empty");

  const items = popup.getItems().map(e => e.label);
  const expectedItems = ["testBugAA", "testBugBB"];
  is(
    items.join("-"),
    expectedItems.join("-"),
    "getItems returns the items we expect"
  );

  info("press Tab and wait for popup to hide");
  onPopupClose = popup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Tab");
  await onPopupClose;

  // At this point the completion suggestion should be accepted.
  ok(!popup.isOpen, "popup is not open");
  checkInputValueAndCursorPosition(
    hud,
    "dump(window.testBugBB|)",
    "completion was successful after VK_TAB"
  );
  ok(!getInputCompletionValue(hud), "there is no completion text");

  info("Test ENTER key when popup is visible with a selected item");
  await setInitialState(hud);
  info("press Enter and wait for popup to hide");
  onPopupClose = popup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Enter");
  await onPopupClose;

  ok(!popup.isOpen, "popup is not open");
  checkInputValueAndCursorPosition(
    hud,
    "dump(window.testBugAA|)",
    "completion was successful after Enter"
  );
  ok(!getInputCompletionValue(hud), "there is no completion text");

  info("Test autocomplete inside parens");
  setInputValue(hud, "dump()");
  EventUtils.synthesizeKey("KEY_ArrowLeft");
  let onAutocompleteUpdated = jsterm.once("autocomplete-updated");
  EventUtils.sendString("window.testBugA");
  await onAutocompleteUpdated;
  ok(popup.isOpen, "popup is open");
  ok(!getInputCompletionValue(hud), "there is no completion text");

  info("Matching the completion proposal should close the popup");
  onPopupClose = popup.once("popup-closed");
  EventUtils.sendString("A");
  await onPopupClose;

  info("Test TAB key when there is no autocomplete suggestion");
  ok(!popup.isOpen, "popup is not open");
  ok(!getInputCompletionValue(hud), "there is no completion text");

  EventUtils.synthesizeKey("KEY_Tab");
  checkInputValueAndCursorPosition(
    hud,
    "dump(window.testBugAA\t|)",
    "completion was successful after Enter"
  );

  info("Check that we don't show the popup when editing words");
  await setInputValueForAutocompletion(hud, "estBug", 0);
  onAutocompleteUpdated = jsterm.once("autocomplete-updated");
  EventUtils.sendString("t");
  await onAutocompleteUpdated;
  is(getInputValue(hud), "testBug", "jsterm has expected value");
  is(popup.isOpen, false, "popup is not open");
  ok(!getInputCompletionValue(hud), "there is no completion text");

  await setInputValueForAutocompletion(hud, "__foo", 1);
  onAutocompleteUpdated = jsterm.once("autocomplete-updated");
  EventUtils.sendString("t");
  await onAutocompleteUpdated;
  is(getInputValue(hud), "_t_foo", "jsterm has expected value");
  is(popup.isOpen, false, "popup is not open");
  ok(!getInputCompletionValue(hud), "there is no completion text");

  await setInputValueForAutocompletion(hud, "$$bar", 1);
  onAutocompleteUpdated = jsterm.once("autocomplete-updated");
  EventUtils.sendString("t");
  await onAutocompleteUpdated;
  is(getInputValue(hud), "$t$bar", "jsterm has expected value");
  is(popup.isOpen, false, "popup is not open");
  ok(!getInputCompletionValue(hud), "there is no completion text");

  await setInputValueForAutocompletion(hud, "99luftballons", 1);
  onAutocompleteUpdated = jsterm.once("autocomplete-updated");
  EventUtils.sendString("t");
  await onAutocompleteUpdated;
  is(getInputValue(hud), "9t9luftballons", "jsterm has expected value");
  is(popup.isOpen, false, "popup is not open");
  ok(!getInputCompletionValue(hud), "there is no completion text");
});

async function setInitialState(hud) {
  const { jsterm } = hud;
  jsterm.focus();
  setInputValue(hud, "dump()");
  EventUtils.synthesizeKey("KEY_ArrowLeft");

  const onPopUpOpen = jsterm.autocompletePopup.once("popup-opened");
  EventUtils.sendString("window.testB");
  checkInputValueAndCursorPosition(hud, "dump(window.testB|)");
  await onPopUpOpen;
}
