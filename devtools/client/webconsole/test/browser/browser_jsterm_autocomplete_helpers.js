/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the autocompletion results contain the names of JSTerm helpers.
// See Bug 686937.

"use strict";

const TEST_URI =
  "data:text/html;charset=utf8,<p>test JSTerm Helpers autocomplete";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  await testInspectAutoCompletion(hud, "i", true);
  await testInspectAutoCompletion(hud, "window.", false);
  await testInspectAutoCompletion(hud, "dump(i", true);
  await testInspectAutoCompletion(hud, "window.dump(i", true);
});

async function testInspectAutoCompletion(hud, inputValue, expectInspect) {
  setInputValue(hud, "");
  const { jsterm } = hud;
  jsterm.focus();
  const updated = jsterm.once("autocomplete-updated");
  EventUtils.sendString(inputValue);
  await updated;
  is(
    getPopupItemsLabel(jsterm.autocompletePopup).includes("inspect"),
    expectInspect,
    `autocomplete results${
      expectInspect ? "" : " does not"
    } contain helper 'inspect'`
  );
}

function getPopupItemsLabel(popup) {
  return popup.getItems().map(item => item.label);
}
