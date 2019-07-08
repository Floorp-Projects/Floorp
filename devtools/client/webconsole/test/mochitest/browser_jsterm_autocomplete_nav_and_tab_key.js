/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// See Bug 585991.

const TEST_URI = `data:text/html;charset=utf-8,
<head>
  <script>
    /* Create a prototype-less object so popup does not contain native
     * Object prototype properties.
     */
    window.foo = Object.create(null, Object.getOwnPropertyDescriptors({
      item00: "value0",
      item1: "value1",
      item2: "value2",
      item3: "value3",
    }));
  </script>
</head>
<body>bug 585991 - autocomplete popup navigation and tab key usage test</body>`;

add_task(async function() {
  // Run test with legacy JsTerm
  await pushPref("devtools.webconsole.jsterm.codeMirror", false);
  await performTests();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTests();
});

async function performTests() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;
  info("web console opened");

  const { autocompletePopup: popup } = jsterm;

  ok(!popup.isOpen, "popup is not open");

  const onPopUpOpen = popup.once("popup-opened");
  setInputValue(hud, "window.foo");

  // Shows the popup
  EventUtils.sendString(".");
  await onPopUpOpen;

  ok(popup.isOpen, "popup is open");

  const popupItems = popup.getItems().map(e => e.label);
  const expectedPopupItems = ["item00", "item1", "item2", "item3"];

  is(popup.itemCount, expectedPopupItems.length, "popup.itemCount is correct");
  is(
    popupItems.join("-"),
    expectedPopupItems.join("-"),
    "getItems returns the items we expect"
  );
  is(popup.selectedIndex, 0, "Index of the first item is selected.");

  EventUtils.synthesizeKey("KEY_ArrowUp");

  let prefix = getInputValue(hud).replace(/[\S]/g, " ");
  is(popup.selectedIndex, 3, "index 3 is selected");
  is(popup.selectedItem.label, "item3", "item3 is selected");
  checkInputCompletionValue(
    hud,
    prefix + "item3",
    "completeNode.value holds item3"
  );

  EventUtils.synthesizeKey("KEY_ArrowUp");

  is(popup.selectedIndex, 2, "index 2 is selected");
  is(popup.selectedItem.label, "item2", "item2 is selected");
  checkInputCompletionValue(
    hud,
    prefix + "item2",
    "completeNode.value holds item2"
  );

  EventUtils.synthesizeKey("KEY_ArrowDown");

  is(popup.selectedIndex, 3, "index 3 is selected");
  is(popup.selectedItem.label, "item3", "item3 is selected");
  checkInputCompletionValue(
    hud,
    prefix + "item3",
    "completeNode.value holds item3"
  );

  let currentSelectionIndex = popup.selectedIndex;

  EventUtils.synthesizeKey("KEY_PageUp");
  ok(
    popup.selectedIndex < currentSelectionIndex,
    "Index is less after Page UP"
  );

  currentSelectionIndex = popup.selectedIndex;
  EventUtils.synthesizeKey("KEY_PageDown");
  ok(
    popup.selectedIndex > currentSelectionIndex,
    "Index is greater after PGDN"
  );

  EventUtils.synthesizeKey("KEY_Home");
  is(popup.selectedIndex, 0, "index is first after Home");

  EventUtils.synthesizeKey("KEY_End");
  is(
    popup.selectedIndex,
    expectedPopupItems.length - 1,
    "index is last after End"
  );

  info("press Tab and wait for popup to hide");
  const onPopupClose = popup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Tab");

  await onPopupClose;

  // At this point the completion suggestion should be accepted.
  ok(!popup.isOpen, "popup is not open");
  is(
    getInputValue(hud),
    "window.foo.item3",
    "completion was successful after KEY_Tab"
  );
  ok(!getInputCompletionValue(hud), "completeNode is empty");

  info(
    "Check that hitting Home hides the completion text when the popup is hidden"
  );
  await setInputValueForAutocompletion(hud, "window.foo.item0");
  prefix = getInputValue(hud).replace(/[\S]/g, " ");
  checkInputCompletionValue(
    hud,
    prefix + "0",
    "completeNode has expected value"
  );
  if (Services.appinfo.OS == "Darwin") {
    EventUtils.synthesizeKey("a", { ctrlKey: true });
  } else {
    EventUtils.synthesizeKey("KEY_Home");
  }
  checkInputCompletionValue(
    hud,
    "",
    "completeNode was cleared after hitting Home"
  );

  info(
    "Check that hitting End hides the completion text when the popup is hidden"
  );
  await setInputValueForAutocompletion(hud, "window.foo.item0");
  prefix = getInputValue(hud).replace(/[\S]/g, " ");
  checkInputCompletionValue(
    hud,
    prefix + "0",
    "completeNode has expected value"
  );
  EventUtils.synthesizeKey("KEY_End");
  checkInputCompletionValue(
    hud,
    "",
    "completeNode was cleared after hitting End"
  );
}
