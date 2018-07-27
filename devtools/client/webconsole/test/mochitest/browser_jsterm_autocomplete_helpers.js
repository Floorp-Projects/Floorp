/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the autocompletion results contain the names of JSTerm helpers.
// See Bug 686937.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,<p>test JSTerm Helpers autocomplete";

add_task(async function() {
  // Run test with legacy JsTerm
  await performTests();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTests();
});

async function performTests() {
  const {jsterm} = await openNewTabAndConsole(TEST_URI);
  await testInspectAutoCompletion(jsterm, "i", true);
  await testInspectAutoCompletion(jsterm, "window.", false);
  await testInspectAutoCompletion(jsterm, "dump(i", true);
  await testInspectAutoCompletion(jsterm, "window.dump(i", true);
}

async function testInspectAutoCompletion(jsterm, inputValue, expectInspect) {
  jsterm.setInputValue(inputValue);
  await complete(jsterm);
  is(getPopupItemsLabel(jsterm.autocompletePopup).includes("inspect"), expectInspect,
    `autocomplete results${expectInspect ? "" : " does not"} contain helper 'inspect'`);
}

function complete(jsterm) {
  const updated = jsterm.once("autocomplete-updated");
  jsterm.complete(jsterm.COMPLETE_HINT_ONLY);
  return updated;
}

function getPopupItemsLabel(popup) {
  return popup.getItems().map(item => item.label);
}
