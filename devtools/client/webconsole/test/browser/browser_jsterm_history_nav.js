/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// See Bug 660806. Check that history navigation with the UP/DOWN arrows does not trigger
// autocompletion.

const TEST_URI =
  "data:text/html;charset=utf-8,<p>bug 660806 - history " +
  "navigation must not show the autocomplete popup";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;
  const popup = jsterm.autocompletePopup;

  // The autocomplete popup should never be displayed during the test.
  const onShown = function() {
    ok(false, "popup shown");
  };
  popup.on("popup-opened", onShown);

  await executeAndWaitForMessage(
    hud,
    `window.foobarBug660806 = {
    'location': 'value0',
    'locationbar': 'value1'
  }`,
    "",
    ".result"
  );
  ok(!popup.isOpen, "popup is not open");

  // Let's add this expression in the input history. We don't use setInputValue since
  // it _does_ trigger an autocompletion request in codeMirror JsTerm.
  await executeAndWaitForMessage(
    hud,
    "window.foobarBug660806.location",
    "",
    ".result"
  );

  const onSetInputValue = jsterm.once("set-input-value");
  EventUtils.synthesizeKey("KEY_ArrowUp");
  await onSetInputValue;

  // We don't have an explicit event to wait for here, so we just wait for the next tick
  // before checking the popup status.
  await new Promise(executeSoon);

  is(
    getInputValue(hud),
    "window.foobarBug660806.location",
    "input has expected value"
  );

  ok(!popup.isOpen, "popup is not open");
  popup.off("popup-opened", onShown);
});
