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

add_task(async function () {
  const {jsterm} = await openNewTabAndConsole(TEST_URI);

  ok(!jsterm.getInputValue(), "jsterm.getInputValue() is empty");
  is(jsterm.inputNode.selectionStart, 0);
  is(jsterm.inputNode.selectionEnd, 0);

  testSingleLineInputNavNoHistory(jsterm);
  testMultiLineInputNavNoHistory(jsterm);
  testNavWithHistory(jsterm);
});

function testSingleLineInputNavNoHistory(jsterm) {
  let inputNode = jsterm.inputNode;
  // Single char input
  EventUtils.synthesizeKey("1", {});
  is(inputNode.selectionStart, 1, "caret location after single char input");

  // nav to start/end with ctrl-a and ctrl-e;
  synthesizeLineStartKey();
  is(inputNode.selectionStart, 0,
     "caret location after single char input and ctrl-a");

  synthesizeLineEndKey();
  is(inputNode.selectionStart, 1,
     "caret location after single char input and ctrl-e");

  // Second char input
  EventUtils.synthesizeKey("2", {});
  // nav to start/end with up/down keys; verify behaviour using ctrl-p/ctrl-n
  EventUtils.synthesizeKey("VK_UP", {});
  is(inputNode.selectionStart, 0,
     "caret location after two char input and VK_UP");
  EventUtils.synthesizeKey("VK_DOWN", {});
  is(inputNode.selectionStart, 2,
     "caret location after two char input and VK_DOWN");

  synthesizeLineStartKey();
  is(inputNode.selectionStart, 0,
     "move caret to beginning of 2 char input with ctrl-a");
  synthesizeLineStartKey();
  is(inputNode.selectionStart, 0,
     "no change of caret location on repeat ctrl-a");
  synthesizeLineUpKey();
  is(inputNode.selectionStart, 0,
     "no change of caret location on ctrl-p from beginning of line");

  synthesizeLineEndKey();
  is(inputNode.selectionStart, 2,
     "move caret to end of 2 char input with ctrl-e");
  synthesizeLineEndKey();
  is(inputNode.selectionStart, 2,
     "no change of caret location on repeat ctrl-e");
  synthesizeLineDownKey();
  is(inputNode.selectionStart, 2,
     "no change of caret location on ctrl-n from end of line");

  synthesizeLineUpKey();
  is(inputNode.selectionStart, 0, "ctrl-p moves to start of line");

  synthesizeLineDownKey();
  is(inputNode.selectionStart, 2, "ctrl-n moves to end of line");
}

function testMultiLineInputNavNoHistory(jsterm) {
  let inputNode = jsterm.inputNode;
  let lineValues = ["one", "2", "something longer", "", "", "three!"];
  jsterm.setInputValue("");
  // simulate shift-return
  for (let i = 0; i < lineValues.length; i++) {
    jsterm.setInputValue(jsterm.getInputValue() + lineValues[i]);
    EventUtils.synthesizeKey("VK_RETURN", { shiftKey: true });
  }
  let inputValue = jsterm.getInputValue();
  is(inputNode.selectionStart, inputNode.selectionEnd);
  is(inputNode.selectionStart, inputValue.length,
     "caret at end of multiline input");

  // possibility newline is represented by one ('\r', '\n') or two
  // ('\r\n') chars
  let newlineString = inputValue.match(/(\r\n?|\n\r?)$/)[0];

  // Ok, test navigating within the multi-line string!
  EventUtils.synthesizeKey("VK_UP", {});
  let expectedStringAfterCarat = lineValues[5] + newlineString;
  is(jsterm.getInputValue().slice(inputNode.selectionStart), expectedStringAfterCarat,
     "up arrow from end of multiline");

  EventUtils.synthesizeKey("VK_DOWN", {});
  is(jsterm.getInputValue().slice(inputNode.selectionStart), "",
     "down arrow from within multiline");

  // navigate up through input lines
  synthesizeLineUpKey();
  is(jsterm.getInputValue().slice(inputNode.selectionStart), expectedStringAfterCarat,
     "ctrl-p from end of multiline");

  for (let i = 4; i >= 0; i--) {
    synthesizeLineUpKey();
    expectedStringAfterCarat = lineValues[i] + newlineString +
      expectedStringAfterCarat;
    is(jsterm.getInputValue().slice(inputNode.selectionStart),
      expectedStringAfterCarat, "ctrl-p from within line " + i +
      " of multiline input");
  }
  synthesizeLineUpKey();
  is(inputNode.selectionStart, 0, "reached start of input");
  is(jsterm.getInputValue(), inputValue,
     "no change to multiline input on ctrl-p from beginning of multiline");

  // navigate to end of first line
  synthesizeLineEndKey();
  let caretPos = inputNode.selectionStart;
  let expectedStringBeforeCarat = lineValues[0];
  is(jsterm.getInputValue().slice(0, caretPos), expectedStringBeforeCarat,
     "ctrl-e into multiline input");
  synthesizeLineEndKey();
  is(inputNode.selectionStart, caretPos,
     "repeat ctrl-e doesn't change caret position in multiline input");

  // navigate down one line; ctrl-a to the beginning; ctrl-e to end
  for (let i = 1; i < lineValues.length; i++) {
    synthesizeLineDownKey();
    synthesizeLineStartKey();
    caretPos = inputNode.selectionStart;
    expectedStringBeforeCarat += newlineString;
    is(jsterm.getInputValue().slice(0, caretPos), expectedStringBeforeCarat,
       "ctrl-a to beginning of line " + (i + 1) + " in multiline input");

    synthesizeLineEndKey();
    caretPos = inputNode.selectionStart;
    expectedStringBeforeCarat += lineValues[i];
    is(jsterm.getInputValue().slice(0, caretPos), expectedStringBeforeCarat,
       "ctrl-e to end of line " + (i + 1) + "in multiline input");
  }
}

function testNavWithHistory(jsterm) {
  let inputNode = jsterm.inputNode;

  // NOTE: Tests does NOT currently define behaviour for ctrl-p/ctrl-n with
  // caret placed _within_ single line input
  let values = [
    '"single line input"',
    '"a longer single-line input to check caret repositioning"',
    '"multi-line"\n"input"\n"here!"',
  ];

  // submit to history
  for (let i = 0; i < values.length; i++) {
    jsterm.setInputValue(values[i]);
    jsterm.execute();
  }
  is(inputNode.selectionStart, 0, "caret location at start of empty line");

  synthesizeLineUpKey();
  is(inputNode.selectionStart, values[values.length - 1].length,
     "caret location correct at end of last history input");

  // Navigate backwards history with ctrl-p
  for (let i = values.length - 1; i > 0; i--) {
    let match = values[i].match(/(\n)/g);
    if (match) {
      // multi-line inputs won't update from history unless caret at beginning
      synthesizeLineStartKey();
      for (let j = 0; j < match.length; j++) {
        synthesizeLineUpKey();
      }
      synthesizeLineUpKey();
    } else {
      // single-line inputs will update from history from end of line
      synthesizeLineUpKey();
    }
    is(jsterm.getInputValue(), values[i - 1],
       "ctrl-p updates inputNode from backwards history values[" + i - 1 + "]");
  }

  let inputValue = jsterm.getInputValue();
  synthesizeLineUpKey();
  is(inputNode.selectionStart, 0,
     "ctrl-p at beginning of history moves caret location to beginning " +
     "of line");
  is(jsterm.getInputValue(), inputValue,
     "no change to input value on ctrl-p from beginning of line");

  // Navigate forwards history with ctrl-n
  for (let i = 1; i < values.length; i++) {
    synthesizeLineDownKey();
    is(jsterm.getInputValue(), values[i],
       "ctrl-n updates inputNode from forwards history values[" + i + "]");
    is(inputNode.selectionStart, values[i].length,
       "caret location correct at end of history input for values[" + i + "]");
  }
  synthesizeLineDownKey();
  ok(!jsterm.getInputValue(), "ctrl-n at end of history updates to empty input");

  // Simulate editing multi-line
  inputValue = "one\nlinebreak";
  jsterm.setInputValue(inputValue);

  // Attempt nav within input
  synthesizeLineUpKey();
  is(jsterm.getInputValue(), inputValue,
     "ctrl-p from end of multi-line does not trigger history");

  synthesizeLineStartKey();
  synthesizeLineUpKey();
  is(jsterm.getInputValue(), values[values.length - 1],
     "ctrl-p from start of multi-line triggers history");
}

function synthesizeLineStartKey() {
  EventUtils.synthesizeKey("a", { ctrlKey: true });
}

function synthesizeLineEndKey() {
  EventUtils.synthesizeKey("e", { ctrlKey: true });
}

function synthesizeLineUpKey() {
  EventUtils.synthesizeKey("p", { ctrlKey: true });
}

function synthesizeLineDownKey() {
  EventUtils.synthesizeKey("n", { ctrlKey: true });
}
