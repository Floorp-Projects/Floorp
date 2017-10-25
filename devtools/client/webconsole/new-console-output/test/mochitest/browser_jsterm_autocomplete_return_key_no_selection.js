/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// See Bug 873250.

const TEST_URI = `data:text/html;charset=utf-8,
<head>
  <script>
    window.testBugA = "hello world";
    window.testBugB = "hello world 2";
  </script>
</head>
<body>bug 873250 - test pressing return with open popup, but no selection</body>`;

add_task(async function () {
  let { jsterm } = await openNewTabAndConsole(TEST_URI);
  // Clearing history that might have been set in previous tests.
  await jsterm.clearHistory();

  const {
    autocompletePopup: popup,
    completeNode,
    inputNode,
  } = jsterm;

  const onPopUpOpen = popup.once("popup-opened");

  info("wait for popup to show");
  jsterm.setInputValue("window.testBu");
  EventUtils.synthesizeKey("g", {});

  await onPopUpOpen;

  ok(popup.isOpen, "popup is open");
  is(popup.itemCount, 2, "popup.itemCount is correct");
  isnot(popup.selectedIndex, -1, "popup.selectedIndex is correct");

  info("press Return and wait for popup to hide");
  const onPopUpClose = popup.once("popup-closed");
  executeSoon(() => EventUtils.synthesizeKey("VK_RETURN", {}));
  await onPopUpClose;

  ok(!popup.isOpen, "popup is not open after VK_RETURN");
  is(jsterm.getInputValue(), "", "inputNode is empty after VK_RETURN");
  is(completeNode.value, "", "completeNode is empty");
  is(jsterm.history[jsterm.history.length - 1], "window.testBug",
     "jsterm history is correct");

  // Cleaning history.
  await jsterm.clearHistory();
});
