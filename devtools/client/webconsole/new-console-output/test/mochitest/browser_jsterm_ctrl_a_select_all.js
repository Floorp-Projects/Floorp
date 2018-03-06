/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from head.js */

// Map Control + A to Select All, In the web console input

/* import-globals-from head.js */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Test console select all";

add_task(async function testCtrlA() {
  let hud = await openNewTabAndConsole(TEST_URI);

  let jsterm = hud.jsterm;
  jsterm.setInputValue("Ignore These Four Words");
  let inputNode = jsterm.inputNode;

  // Test select all with (cmd|control) + a.
  EventUtils.synthesizeKey("a", { accelKey: true });

  let inputLength = inputNode.selectionEnd - inputNode.selectionStart;
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
