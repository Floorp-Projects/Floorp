/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the jsterm input supports multiline values. See Bug 588967.

const TEST_URI = "data:text/html;charset=utf-8,Test for jsterm multine input";

add_task(async function() {
  // Run test with legacy JsTerm
  await performTests();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTests();
});

async function performTests() {
  const {jsterm, ui} = await openNewTabAndConsole(TEST_URI);
  const inputContainer = ui.window.document.querySelector(".jsterm-input-container");

  const ordinaryHeight = inputContainer.clientHeight;

  // Set a multiline value
  jsterm.setInputValue("hello\nworld\n");
  ok(inputContainer.clientHeight > ordinaryHeight, "the input expanded");

  info("Erase the value and test if the inputNode shrinks again");
  jsterm.setInputValue("");
  is(inputContainer.clientHeight, ordinaryHeight, "the input's height is normal again");
}
