/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// See Bug 660806. Check that history navigation with the UP/DOWN arrows does not trigger
// autocompletion.

const TEST_URI = "data:text/html;charset=utf-8,<p>bug 660806 - history " +
                 "navigation must not show the autocomplete popup";

add_task(async function () {
  let { jsterm } = await openNewTabAndConsole(TEST_URI);
  let popup = jsterm.autocompletePopup;

  // The autocomplete popup should never be displayed during the test.
  let onShown = function () {
    ok(false, "popup shown");
  };

  jsterm.execute(`window.foobarBug660806 = {
    'location': 'value0',
    'locationbar': 'value1'
  }`);

  popup.on("popup-opened", onShown);

  ok(!popup.isOpen, "popup is not open");

  ok(!jsterm.lastInputValue, "no lastInputValue");
  jsterm.setInputValue("window.foobarBug660806.location");
  is(jsterm.lastInputValue, "window.foobarBug660806.location",
     "lastInputValue is correct");

  EventUtils.synthesizeKey("VK_RETURN", {});

  let onSetInputValue = jsterm.once("set-input-value");
  EventUtils.synthesizeKey("VK_UP", {});
  await onSetInputValue;

  // We don't have an explicit event to wait for here, so we just wait for the next tick
  // before checking the popup status.
  await new Promise(executeSoon);

  is(jsterm.lastInputValue, "window.foobarBug660806.location",
     "lastInputValue is correct, again");

  ok(!popup.isOpen, "popup is not open");
  popup.off("popup-opened", onShown);
});
