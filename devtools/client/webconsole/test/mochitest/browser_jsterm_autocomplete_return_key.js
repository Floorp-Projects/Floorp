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
    window.foobar = Object.create(null);
    Object.assign(window.foobar, {
      item0: "value0",
      item1: "value1",
      item2: "value2",
      item3: "value3",
    });
  </script>
</head>
<body>bug 585991 - test pressing return with open popup</body>`;

add_task(async function() {
  // Run test with legacy JsTerm
  await performTests();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTests();
});

async function performTests() {
  const { jsterm } = await openNewTabAndConsole(TEST_URI);
  const { autocompletePopup: popup } = jsterm;

  const onPopUpOpen = popup.once("popup-opened");

  info("wait for completion suggestions: window.foobar.");

  jsterm.setInputValue("window.fooba");
  EventUtils.sendString("r.");

  await onPopUpOpen;

  ok(popup.isOpen, "popup is open");

  const expectedPopupItems = [
    "item3",
    "item2",
    "item1",
    "item0",
  ];
  is(popup.itemCount, expectedPopupItems.length, "popup.itemCount is correct");
  is(popup.selectedIndex, expectedPopupItems.length - 1,
    "First index from bottom is selected");

  EventUtils.synthesizeKey("KEY_ArrowDown");

  is(popup.selectedIndex, 0, "index 0 is selected");
  is(popup.selectedItem.label, "item3", "item3 is selected");
  const prefix = jsterm.getInputValue().replace(/[\S]/g, " ");
  checkJsTermCompletionValue(jsterm, prefix + "item3", "completeNode.value holds item3");

  info("press Return to accept suggestion. wait for popup to hide");
  const onPopupClose = popup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Enter");

  await onPopupClose;

  ok(!popup.isOpen, "popup is not open after KEY_Enter");
  is(jsterm.getInputValue(), "window.foobar.item3",
    "completion was successful after KEY_Enter");
  ok(!getJsTermCompletionValue(jsterm), "completeNode is empty");
}
