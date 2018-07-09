/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// See Bug 583816.

const TEST_URI = "data:text/html,Testing jsterm with no input";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  testCompletion(hud);
});

function testCompletion(hud) {
  const jsterm = hud.jsterm;
  const input = jsterm.inputNode;

  // With empty input, tab through
  jsterm.setInputValue("");
  EventUtils.synthesizeKey("KEY_Tab");
  is(jsterm.getInputValue(), "", "inputnode is empty - matched");
  ok(!hasFocus(input), "input isn't focused anymore");
  jsterm.focus();

  // With non-empty input, insert a tab
  jsterm.setInputValue("window.Bug583816");
  EventUtils.synthesizeKey("KEY_Tab");
  is(jsterm.getInputValue(), "window.Bug583816\t",
     "input content - matched");
  ok(hasFocus(input), "input is still focused");
}
