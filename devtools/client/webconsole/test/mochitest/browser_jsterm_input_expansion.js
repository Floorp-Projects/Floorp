/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the jsterm input supports multiline values. See Bug 588967.

const TEST_URI = "data:text/html;charset=utf-8,Test for jsterm multine input";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  const input = hud.jsterm.inputNode;

  info("Focus the jsterm input");
  input.focus();

  const ordinaryHeight = input.clientHeight;

  // Set a multiline value
  input.value = "hello\nworld\n";

  // Set the caret at the end of input
  const length = input.value.length;
  input.selectionEnd = length;
  input.selectionStart = length;

  info("Type 'd' in jsterm to trigger height change for the input");
  EventUtils.sendString("d");
  ok(input.clientHeight > ordinaryHeight, "the input expanded");

  info("Erase the value and test if the inputNode shrinks again");
  input.value = "";
  EventUtils.sendString("d");
  is(input.clientHeight, ordinaryHeight, "the input's height is normal again");
});
