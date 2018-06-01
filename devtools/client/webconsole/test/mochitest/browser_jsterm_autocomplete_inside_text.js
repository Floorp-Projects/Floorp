/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// See Bug 812618.

const TEST_URI = `data:text/html;charset=utf-8,
<head>
  <script>
    window.testBugA = "hello world";
    window.testBugB = "hello world 2";
  </script>
</head>
<body>bug 812618 - test completion inside text</body>`;

add_task(async function() {
  const { jsterm } = await openNewTabAndConsole(TEST_URI);
  info("web console opened");

  const {
    autocompletePopup: popup,
    completeNode,
    inputNode,
  } = jsterm;

  const onPopUpOpen = popup.once("popup-opened");

  const dumpString = "dump(window.testBu)";
  jsterm.setInputValue(dumpString);
  inputNode.selectionStart = inputNode.selectionEnd = dumpString.indexOf(")");
  EventUtils.sendString("g");

  await onPopUpOpen;

  ok(popup.isOpen, "popup is open");
  is(popup.itemCount, 2, "popup.itemCount is correct");

  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(popup.selectedIndex, 0, "popup.selectedIndex is correct");
  ok(!completeNode.value, "completeNode.value is empty");

  const items = popup.getItems().map(e => e.label);
  const expectedItems = ["testBugB", "testBugA"];
  is(items.join("-"), expectedItems.join("-"), "getItems returns the items we expect");

  info("press Tab and wait for popup to hide");
  const onPopupClose = popup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Tab");

  await onPopupClose;

  // At this point the completion suggestion should be accepted.
  ok(!popup.isOpen, "popup is not open");
  const expectedInput = "dump(window.testBugB)";
  is(jsterm.getInputValue(), expectedInput, "completion was successful after VK_TAB");
  is(inputNode.selectionStart, expectedInput.length - 1, "cursor location is correct");
  is(inputNode.selectionStart, inputNode.selectionEnd, "cursor location (confirmed)");
  ok(!completeNode.value, "completeNode is empty");
});
