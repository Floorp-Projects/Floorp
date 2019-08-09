/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the Enter keys works as expected. See Bug 585991 and 1483880.

const TEST_URI = `data:text/html;charset=utf-8,
<head>
  <script>
    /* Create a prototype-less object so popup does not contain native
     * Object prototype properties.
     */
    window.foobar = Object.create(null);
    Object.assign(window.foobar, {
      item0: "value0",
      item1: "value1",
      item2: "value2",
      item3: "value3",
      item33: "value33",
    });
  </script>
</head>
<body>bug 585991 - test pressing return with open popup</body>`;

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;
  const { autocompletePopup: popup } = jsterm;

  let onPopUpOpen = popup.once("popup-opened");

  info("wait for completion suggestions: window.foobar.");

  setInputValue(hud, "window.fooba");
  EventUtils.sendString("r.");

  await onPopUpOpen;

  ok(popup.isOpen, "popup is open");

  const expectedPopupItems = ["item0", "item1", "item2", "item3", "item33"];
  is(popup.itemCount, expectedPopupItems.length, "popup.itemCount is correct");
  is(popup.selectedIndex, 0, "First index from top is selected");

  EventUtils.synthesizeKey("KEY_ArrowUp");

  is(
    popup.selectedIndex,
    expectedPopupItems.length - 1,
    "last index is selected"
  );
  is(popup.selectedItem.label, "item33", "item33 is selected");
  checkInputCompletionValue(hud, "item33", "completeNode.value holds item33");

  info("press Return to accept suggestion. wait for popup to hide");
  let onPopupClose = popup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Enter");

  await onPopupClose;

  ok(!popup.isOpen, "popup is not open after KEY_Enter");
  is(
    getInputValue(hud),
    "window.foobar.item33",
    "completion was successful after KEY_Enter"
  );
  ok(!getInputCompletionValue(hud), "completeNode is empty");

  info(
    "Test that hitting enter when the completeNode is empty closes the popup"
  );
  onPopUpOpen = popup.once("popup-opened");
  info("wait for completion suggestions: window.foobar.item3");
  setInputValue(hud, "window.foobar.item");
  EventUtils.sendString("3");
  await onPopUpOpen;

  is(popup.selectedItem.label, "item3", "item3 is selected");
  ok(!getInputCompletionValue(hud), "completeNode is empty");

  onPopupClose = popup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Enter");
  await onPopupClose;

  ok(!popup.isOpen, "popup is not open after KEY_Enter");
  is(
    getInputValue(hud),
    "window.foobar.item3",
    "completion was successful after KEY_Enter"
  );
});
