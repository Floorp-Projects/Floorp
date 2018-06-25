/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the console history feature accessed via the up and down arrow keys.

"use strict";

const TEST_URI = "data:text/html;charset=UTF-8,test";
const COMMANDS = ["document", "window", "window.location"];

const {
  HISTORY_BACK,
  HISTORY_FORWARD,
} = require("devtools/client/webconsole/constants");

add_task(async function() {
  // Run test with legacy JsTerm
  await testHistory();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await testHistory();
});

async function testHistory() {
  const { jsterm, ui } = await openNewTabAndConsole(TEST_URI);
  ui.clearOutput();

  for (const command of COMMANDS) {
    info(`Executing command ${command}`);
    jsterm.setInputValue(command);
    await jsterm.execute();
  }

  for (let x = COMMANDS.length - 1; x != -1; x--) {
    jsterm.historyPeruse(HISTORY_BACK);
    is(jsterm.getInputValue(), COMMANDS[x], "check history previous idx:" + x);
  }

  jsterm.historyPeruse(HISTORY_BACK);
  is(jsterm.getInputValue(), COMMANDS[0], "test that item is still index 0");

  jsterm.historyPeruse(HISTORY_BACK);
  is(jsterm.getInputValue(), COMMANDS[0], "test that item is still still index 0");

  for (let i = 1; i < COMMANDS.length; i++) {
    jsterm.historyPeruse(HISTORY_FORWARD);
    is(jsterm.getInputValue(), COMMANDS[i], "check history next idx:" + i);
  }

  jsterm.historyPeruse(HISTORY_FORWARD);
  is(jsterm.getInputValue(), "", "check input is empty again");

  // Simulate pressing Arrow_Down a few times and then if Arrow_Up shows
  // the previous item from history again.
  jsterm.historyPeruse(HISTORY_FORWARD);
  jsterm.historyPeruse(HISTORY_FORWARD);
  jsterm.historyPeruse(HISTORY_FORWARD);

  is(jsterm.getInputValue(), "", "check input is still empty");

  const idxLast = COMMANDS.length - 1;
  jsterm.historyPeruse(HISTORY_BACK);
  is(jsterm.getInputValue(), COMMANDS[idxLast], "check history next idx:" + idxLast);
}
