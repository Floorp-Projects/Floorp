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

// We should turn off auto-multiline editing during these tests
const PREF_AUTO_MULTILINE = "devtools.webconsole.autoMultiline";

add_task(async function () {
  Services.prefs.setBoolPref(PREF_AUTO_MULTILINE, false);

  let { jsterm } = await openNewTabAndConsole(TEST_URI);
  const {
    autocompletePopup: popup,
    completeNode,
    inputNode,
  } = jsterm;

  let onPopUpOpen = popup.once("popup-opened");

  info("wait for completion suggestions: window.foobar.");

  jsterm.setInputValue("window.fooba");
  EventUtils.synthesizeKey("r", {});
  EventUtils.synthesizeKey(".", {});

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

  EventUtils.synthesizeKey("VK_DOWN", {});

  is(popup.selectedIndex, 0, "index 0 is selected");
  is(popup.selectedItem.label, "item3", "item3 is selected");
  let prefix = jsterm.getInputValue().replace(/[\S]/g, " ");
  is(completeNode.value, prefix + "item3", "completeNode.value holds item3");

  info("press Return to accept suggestion. wait for popup to hide");
  const onPopupClose = popup.once("popup-closed");
  EventUtils.synthesizeKey("VK_RETURN", {});

  await onPopupClose;

  ok(!popup.isOpen, "popup is not open after VK_RETURN");
  is(jsterm.getInputValue(), "window.foobar.item3",
    "completion was successful after VK_RETURN");
  ok(!completeNode.value, "completeNode is empty");

  Services.prefs.clearUserPref(PREF_AUTO_MULTILINE);
});
