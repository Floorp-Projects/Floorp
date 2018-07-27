/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test navigation of webconsole contents via ctrl-a, ctrl-e, ctrl-p, ctrl-n
// see https://bugzilla.mozilla.org/show_bug.cgi?id=804845
//
// The shortcuts tested here have platform limitations:
// - ctrl-e does not work on windows,
// - ctrl-a, ctrl-p and ctrl-n only work on OSX
"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console test for " +
                 "bug 804845 and bug 619598";

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
  checkJsTermCursor(jsterm, 0, "Cursor is at the start of the input");

  testSingleLineInputNavNoHistory(jsterm);
  testMultiLineInputNavNoHistory(jsterm);
  await testNavWithHistory(jsterm);
}

function testSingleLineInputNavNoHistory(jsterm) {
  const checkInput = (expected, assertionInfo) =>
    checkJsTermValueAndCursor(jsterm, expected, assertionInfo);

  // Single char input
  EventUtils.sendString("1");
  checkInput("1|", "caret location after single char input");

  // nav to start/end with ctrl-a and ctrl-e;
  synthesizeLineStartKey();
  checkInput("|1", "caret location after single char input and ctrl-a");

  synthesizeLineEndKey();
  checkInput("1|", "caret location after single char input and ctrl-e");

  // Second char input
  EventUtils.sendString("2");
  checkInput("12|", "caret location after second char input");

  // nav to start/end with up/down keys; verify behaviour using ctrl-p/ctrl-n
  EventUtils.synthesizeKey("KEY_ArrowUp");
  checkInput("|12", "caret location after two char input and KEY_ArrowUp");

  EventUtils.synthesizeKey("KEY_ArrowDown");
  checkInput("12|", "caret location after two char input and KEY_ArrowDown");

  synthesizeLineStartKey();
  checkInput("|12", "move caret to beginning of 2 char input with ctrl-a");

  synthesizeLineStartKey();
  checkInput("|12", "no change of caret location on repeat ctrl-a");

  synthesizeLineUpKey();
  checkInput("|12", "no change of caret location on ctrl-p from beginning of line");

  synthesizeLineEndKey();
  checkInput("12|", "move caret to end of 2 char input with ctrl-e");

  synthesizeLineEndKey();
  checkInput("12|", "no change of caret location on repeat ctrl-e");

  synthesizeLineDownKey();
  checkInput("12|", "no change of caret location on ctrl-n from end of line");

  synthesizeLineUpKey();
  checkInput("|12", "ctrl-p moves to start of line");

  synthesizeLineDownKey();
  checkInput("12|", "ctrl-n moves to end of line");
}

function testMultiLineInputNavNoHistory(jsterm) {
  const checkInput = (expected, assertionInfo) =>
    checkJsTermValueAndCursor(jsterm, expected, assertionInfo);

  const lineValues = ["one", "2", "something longer", "", "", "three!"];
  jsterm.setInputValue("");
  // simulate shift-return
  for (const lineValue of lineValues) {
    jsterm.setInputValue(jsterm.getInputValue() + lineValue);
    EventUtils.synthesizeKey("KEY_Enter", {shiftKey: true});
  }

  checkInput(
`one
2
something longer


three!
|`, "caret at end of multiline input");

  // Ok, test navigating within the multi-line string!
  EventUtils.synthesizeKey("KEY_ArrowUp");
  checkInput(
`one
2
something longer


|three!
`, "up arrow from end of multiline");

  EventUtils.synthesizeKey("KEY_ArrowDown");
  checkInput(
`one
2
something longer


three!
|`, "down arrow from within multiline");

  // navigate up through input lines
  synthesizeLineUpKey();
  checkInput(
`one
2
something longer


|three!
`, "ctrl-p from end of multiline");

  for (let i = 0; i < 5; i++) {
    synthesizeLineUpKey();
  }

  checkInput(
`|one
2
something longer


three!
`, "reached start of input");

  synthesizeLineUpKey();
  checkInput(
`|one
2
something longer


three!
`, "no change to multiline input on ctrl-p from beginning of multiline");

  // navigate to end of first line
  synthesizeLineEndKey();
  checkInput(
`one|
2
something longer


three!
`, "ctrl-e into multiline input");

  synthesizeLineEndKey();
  checkInput(
`one|
2
something longer


three!
`, "repeat ctrl-e doesn't change caret position in multiline input");

  synthesizeLineDownKey();
  synthesizeLineStartKey();
  checkInput(
`one
|2
something longer


three!
`);

  synthesizeLineEndKey();
  synthesizeLineDownKey();
  synthesizeLineStartKey();
  checkInput(
`one
2
|something longer


three!
`);
}

async function testNavWithHistory(jsterm) {
  const checkInput = (expected, assertionInfo) =>
    checkJsTermValueAndCursor(jsterm, expected, assertionInfo);

  // NOTE: Tests does NOT currently define behaviour for ctrl-p/ctrl-n with
  // caret placed _within_ single line input
  const values = [
    "single line input",
    "a longer single-line input to check caret repositioning",
    "multi-line\ninput\nhere",
  ];

  // submit to history
  for (const value of values) {
    jsterm.setInputValue(value);
    await jsterm.execute();
  }

  checkInput("|", "caret location at start of empty line");

  synthesizeLineUpKey();
  checkInput("multi-line\ninput\nhere|", "caret location at end of last history input");

  synthesizeLineStartKey();
  checkInput("multi-line\ninput\n|here",
    "caret location at beginning of last line of last history input");

  synthesizeLineUpKey();
  checkInput("multi-line\n|input\nhere",
    "caret location at beginning of second line of last history input");

  synthesizeLineUpKey();
  checkInput("|multi-line\ninput\nhere",
    "caret location at beginning of first line of last history input");

  synthesizeLineUpKey();
  checkInput("a longer single-line input to check caret repositioning|",
    "caret location at the end of second history input");

  synthesizeLineUpKey();
  checkInput("single line input|", "caret location at the end of first history input");

  synthesizeLineUpKey();
  checkInput("|single line input",
    "ctrl-p at beginning of history moves caret location to beginning of line");

  synthesizeLineDownKey();
  checkInput("a longer single-line input to check caret repositioning|",
    "caret location at the end of second history input");

  synthesizeLineDownKey();
  checkInput("multi-line\ninput\nhere|", "caret location at end of last history input");

  synthesizeLineDownKey();
  checkInput("|", "ctrl-n at end of history updates to empty input");

  // Simulate editing multi-line
  const inputValue = "one\nlinebreak";
  jsterm.setInputValue(inputValue);
  checkInput("one\nlinebreak|");

  // Attempt nav within input
  synthesizeLineUpKey();
  checkInput("one|\nlinebreak", "ctrl-p from end of multi-line does not trigger history");

  synthesizeLineStartKey();
  checkInput("|one\nlinebreak");

  synthesizeLineUpKey();
  checkInput("multi-line\ninput\nhere|",
    "ctrl-p from start of multi-line triggers history");
}

function synthesizeLineStartKey() {
  EventUtils.synthesizeKey("a", {ctrlKey: true});
}

function synthesizeLineEndKey() {
  EventUtils.synthesizeKey("e", {ctrlKey: true});
}

function synthesizeLineUpKey() {
  EventUtils.synthesizeKey("p", {ctrlKey: true});
}

function synthesizeLineDownKey() {
  EventUtils.synthesizeKey("n", {ctrlKey: true});
}
