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
  // Run test with legacy JsTerm
  await pushPref("devtools.webconsole.jsterm.codeMirror", false);
  await performTests();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTests();
});

async function performTests() {
  const { jsterm } = await openNewTabAndConsole(TEST_URI);
  info("web console opened");

  const { autocompletePopup: popup } = jsterm;

  await setInitialState(jsterm);

  ok(popup.isOpen, "popup is open");
  is(popup.itemCount, 2, "popup.itemCount is correct");
  is(popup.selectedIndex, 0, "popup.selectedIndex is correct");
  ok(!getJsTermCompletionValue(jsterm), "there is no completion text");

  info("Pressing arrow right");
  let onPopupClose = popup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_ArrowRight");
  await onPopupClose;
  ok(true, "popup was closed");
  let expectedInput = "dump(window.testB)";
  is(jsterm.getInputValue(), expectedInput, "input wasn't modified");
  checkJsTermCursor(jsterm, expectedInput.length, "cursor was moved to the right");

  await setInitialState(jsterm);
  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(popup.selectedIndex, 1, "popup.selectedIndex is correct");
  ok(!getJsTermCompletionValue(jsterm), "completeNode.value is empty");

  const items = popup.getItems().map(e => e.label);
  const expectedItems = ["testBugAA", "testBugBB"];
  is(items.join("-"), expectedItems.join("-"), "getItems returns the items we expect");

  info("press Tab and wait for popup to hide");
  onPopupClose = popup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Tab");
  await onPopupClose;

  // At this point the completion suggestion should be accepted.
  ok(!popup.isOpen, "popup is not open");
  expectedInput = "dump(window.testBugBB)";
  is(jsterm.getInputValue(), expectedInput, "completion was successful after VK_TAB");
  checkJsTermCursor(jsterm, expectedInput.length - 1, "cursor location is correct");
  ok(!getJsTermCompletionValue(jsterm), "there is no completion text");

  info("Test ENTER key when popup is visible with a selected item");
  await setInitialState(jsterm);
  info("press Enter and wait for popup to hide");
  onPopupClose = popup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Enter");
  await onPopupClose;

  ok(!popup.isOpen, "popup is not open");
  expectedInput = "dump(window.testBugAA)";
  is(jsterm.getInputValue(), expectedInput, "completion was successful after Enter");
  checkJsTermCursor(jsterm, expectedInput.length - 1, "cursor location is correct");
  ok(!getJsTermCompletionValue(jsterm), "there is no completion text");

  info("Test TAB key when there is no autocomplete suggestion");
  jsterm.setInputValue("dump()");
  EventUtils.synthesizeKey("KEY_ArrowLeft");
  const onAutocompleteUpdated = jsterm.once("autocomplete-updated");
  EventUtils.sendString("window.testBugA");
  await onAutocompleteUpdated;
  ok(!popup.isOpen, "popup is not open");
  ok(!getJsTermCompletionValue(jsterm), "there is no completion text");

  EventUtils.synthesizeKey("KEY_Tab");

  expectedInput = "dump(window.testBugAA)";
  is(jsterm.getInputValue(), "dump(window.testBugA\t)", "Tab inserted a tab char");
  checkJsTermCursor(jsterm, expectedInput.length - 1, "cursor location is correct");
}

function setInitialState(jsterm) {
  jsterm.focus();
  jsterm.setInputValue("dump()");
  EventUtils.synthesizeKey("KEY_ArrowLeft");

  const onPopUpOpen = jsterm.autocompletePopup.once("popup-opened");
  EventUtils.sendString("window.testB");
  return onPopUpOpen;
}
