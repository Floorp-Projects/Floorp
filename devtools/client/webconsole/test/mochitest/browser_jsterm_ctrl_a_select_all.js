/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Map Control + A to Select All, In the web console input

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Test console select all";

add_task(async function testCtrlA() {
  const hud = await openNewTabAndConsole(TEST_URI);

  const jsterm = hud.jsterm;
  jsterm.setInputValue("Ignore These Four Words");
  const inputNode = jsterm.inputNode;

  // Test select all with (cmd|control) + a.
  EventUtils.synthesizeKey("a", { accelKey: true });

  const inputLength = inputNode.selectionEnd - inputNode.selectionStart;
  is(inputLength, jsterm.getInputValue().length, "Select all of input");

  // (cmd|control) + e cannot be disabled on Linux so skip this section on that
  // OS.
  if (Services.appinfo.OS !== "Linux") {
   // Test do nothing on Control + E.
    jsterm.setInputValue("Ignore These Four Words");
    inputNode.selectionStart = 0;
    EventUtils.synthesizeKey("e", { accelKey: true });
    is(inputNode.selectionStart, 0,
      "control|cmd + e does not move to end of input");
  }
});
