/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the console history feature accessed via the up and down arrow keys.

"use strict";

const TEST_URI = "data:text/html;charset=UTF-8,test";
const COMMANDS = ["document", "window", "window.location"];

add_task(async function() {
  await pushPref("devtools.webconsole.jsterm.codeMirror", false);
  await testHistory();

  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await testHistory();
});

async function testHistory() {
  const { jsterm } = await openNewTabAndConsole(TEST_URI);
  jsterm.focus();

  for (const command of COMMANDS) {
    info(`Executing command ${command}`);
    await jsterm.execute(command);
  }

  for (let x = COMMANDS.length - 1; x != -1; x--) {
    EventUtils.synthesizeKey("KEY_ArrowUp");
    is(jsterm.getInputValue(), COMMANDS[x], "check history previous idx:" + x);
  }

  EventUtils.synthesizeKey("KEY_ArrowUp");
  is(jsterm.getInputValue(), COMMANDS[0], "test that item is still index 0");

  EventUtils.synthesizeKey("KEY_ArrowUp");
  is(jsterm.getInputValue(), COMMANDS[0], "test that item is still still index 0");

  for (let i = 1; i < COMMANDS.length; i++) {
    EventUtils.synthesizeKey("KEY_ArrowDown");
    is(jsterm.getInputValue(), COMMANDS[i], "check history next idx:" + i);
  }

  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(jsterm.getInputValue(), "", "check input is empty again");

  // Simulate pressing Arrow_Down a few times and then if Arrow_Up shows
  // the previous item from history again.
  EventUtils.synthesizeKey("KEY_ArrowDown");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  EventUtils.synthesizeKey("KEY_ArrowDown");

  is(jsterm.getInputValue(), "", "check input is still empty");

  const idxLast = COMMANDS.length - 1;
  EventUtils.synthesizeKey("KEY_ArrowUp");
  is(jsterm.getInputValue(), COMMANDS[idxLast], "check history next idx:" + idxLast);
}
