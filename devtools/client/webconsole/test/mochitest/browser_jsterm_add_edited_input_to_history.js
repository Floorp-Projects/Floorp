/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that user input that is not submitted in the command line input is not
// lost after navigating in history.
// See https://bugzilla.mozilla.org/show_bug.cgi?id=817834

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console test for bug 817834";

add_task(async function() {
  // Run test with legacy JsTerm
  await performTests();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTests();
});

async function performTests() {
  const {jsterm} = await openNewTabAndConsole(TEST_URI);

  ok(!jsterm.getInputValue(), "jsterm.getInputValue() is empty");
  checkJsTermCursor(jsterm, 0, "Cursor is at expected position");

  jsterm.setInputValue('"first item"');
  EventUtils.synthesizeKey("KEY_ArrowUp");
  is(jsterm.getInputValue(), '"first item"', "null test history up");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(jsterm.getInputValue(), '"first item"', "null test history down");

  await jsterm.execute();
  is(jsterm.getInputValue(), "", "cleared input line after submit");

  jsterm.setInputValue('"editing input 1"');
  EventUtils.synthesizeKey("KEY_ArrowUp");
  is(jsterm.getInputValue(), '"first item"', "test history up");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(jsterm.getInputValue(), '"editing input 1"',
    "test history down restores in-progress input");

  jsterm.setInputValue('"second item"');
  await jsterm.execute();
  jsterm.setInputValue('"editing input 2"');
  EventUtils.synthesizeKey("KEY_ArrowUp");
  is(jsterm.getInputValue(), '"second item"', "test history up");
  EventUtils.synthesizeKey("KEY_ArrowUp");
  is(jsterm.getInputValue(), '"first item"', "test history up");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(jsterm.getInputValue(), '"second item"', "test history down");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(jsterm.getInputValue(), '"editing input 2"',
     "test history down restores new in-progress input again");

  // Appending the same value again should not impact the history.
  // Let's also use some spaces around to check that the input value
  // is properly trimmed.
  await jsterm.execute('"second item"');
  await jsterm.execute('  "second item"    ');
  EventUtils.synthesizeKey("KEY_ArrowUp");
  is(jsterm.getInputValue(), '"second item"',
    "test history up reaches duplicated entry just once");
  EventUtils.synthesizeKey("KEY_ArrowUp");
  is(jsterm.getInputValue(), '"first item"',
    "test history up reaches the previous value");
}
