/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// See Bug 873250.

const TEST_URI = `data:text/html;charset=utf-8,<!DOCTYPE html>
<head>
  <script>
    window.testBugA = "hello world";
    window.testBugB = "hello world 2";
  </script>
</head>
<body>bug 873250 - test pressing return with open popup, but no selection</body>`;

const {
  getHistoryEntries,
} = require("resource://devtools/client/webconsole/selectors/history.js");

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm, ui } = hud;

  const { autocompletePopup: popup } = jsterm;

  const onPopUpOpen = popup.once("popup-opened");

  info("wait for popup to show");
  setInputValue(hud, "window.testBu");
  EventUtils.sendString("g");

  await onPopUpOpen;

  ok(popup.isOpen, "popup is open");
  is(popup.itemCount, 2, "popup.itemCount is correct");
  isnot(popup.selectedIndex, -1, "popup.selectedIndex is correct");

  info("press Return and wait for popup to hide");
  const onPopUpClose = popup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Enter");
  await onPopUpClose;

  ok(!popup.isOpen, "popup is not open after KEY_Enter");
  is(
    getInputValue(hud),
    "window.testBugA",
    "input was completed with the first item of the popup"
  );
  ok(!getInputCompletionValue(hud), "completeNode is empty");

  const onMessage = waitForMessageByType(hud, "hello world", ".result");
  EventUtils.synthesizeKey("KEY_Enter");
  is(getInputValue(hud), "", "input is empty after KEY_Enter");

  const state = ui.wrapper.getStore().getState();
  const entries = getHistoryEntries(state);
  is(
    entries[entries.length - 1],
    "window.testBugA",
    "jsterm history is correct"
  );

  info("Wait for the execution value to appear");
  await onMessage;
});
