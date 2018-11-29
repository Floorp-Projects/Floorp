/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// See Bug 585991.

const TEST_URI = `data:text/html;charset=utf-8,Autocomplete await expression`;

add_task(async function() {
  // Run test with legacy JsTerm
  await pushPref("devtools.webconsole.jsterm.codeMirror", false);
  await performTests();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTests();
});

async function performTests() {
  const { jsterm } = await openNewTabAndConsole(TEST_URI);
  const { autocompletePopup } = jsterm;

  info("Check that the await keyword is in the autocomplete");
  await setInputValueForAutocompletion(jsterm, "aw");
  checkJsTermCompletionValue(jsterm, "  ait", "completeNode has expected value");

  EventUtils.synthesizeKey("KEY_Tab");
  is(jsterm.getInputValue(), "await", "'await' tab completion");

  const updated = jsterm.once("autocomplete-updated");
  EventUtils.sendString(" ");
  await updated;

  info("Check that the autocomplete popup is displayed");
  const onPopUpOpen = autocompletePopup.once("popup-opened");
  EventUtils.sendString("P");
  await onPopUpOpen;

  ok(autocompletePopup.isOpen, "popup is open");
  ok(autocompletePopup.items.some(item => item.label === "Promise"),
    "popup has expected `Promise` item");
}
