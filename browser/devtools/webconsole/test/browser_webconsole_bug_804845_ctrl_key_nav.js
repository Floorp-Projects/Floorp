/* -*- Mode: js2; js2-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  zmgmoz <zmgmoz@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

// Test navigation of webconsole contents via ctrl-a, ctrl-e, ctrl-p, ctrl-n
// see https://bugzilla.mozilla.org/show_bug.cgi?id=804845

let jsterm, inputNode;
function test() {
  addTab("data:text/html;charset=utf-8,Web Console test for bug 804845 and bug 619598");
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, doTests);
  }, true);
}

function doTests(HUD) {
  jsterm = HUD.jsterm;
  inputNode = jsterm.inputNode;
  ok(!jsterm.inputNode.value, "inputNode.value is empty");
  is(jsterm.inputNode.selectionStart, 0);
  is(jsterm.inputNode.selectionEnd, 0);

  testSingleLineInputNavNoHistory();
  testMultiLineInputNavNoHistory();
  testNavWithHistory();

  jsterm = inputNode = null;
  executeSoon(finishTest);
}

function testSingleLineInputNavNoHistory() {
  // Single char input
  EventUtils.synthesizeKey("1", {});
  is(inputNode.selectionStart, 1, "caret location after single char input");

  // nav to start/end with ctrl-a and ctrl-e;
  EventUtils.synthesizeKey("a", { ctrlKey: true });
  is(inputNode.selectionStart, 0, "caret location after single char input and ctrl-a");

  EventUtils.synthesizeKey("e", { ctrlKey: true });
  is(inputNode.selectionStart, 1, "caret location after single char input and ctrl-e");

  // Second char input
  EventUtils.synthesizeKey("2", {});
  // nav to start/end with up/down keys; verify behaviour using ctrl-p/ctrl-n
  EventUtils.synthesizeKey("VK_UP", {});
  is(inputNode.selectionStart, 0, "caret location after two char input and VK_UP");
  EventUtils.synthesizeKey("VK_DOWN", {});
  is(inputNode.selectionStart, 2, "caret location after two char input and VK_DOWN");

  EventUtils.synthesizeKey("a", { ctrlKey: true });
  is(inputNode.selectionStart, 0, "move caret to beginning of 2 char input with ctrl-a");
  EventUtils.synthesizeKey("a", { ctrlKey: true });
  is(inputNode.selectionStart, 0, "no change of caret location on repeat ctrl-a");
  EventUtils.synthesizeKey("p", { ctrlKey: true });
  is(inputNode.selectionStart, 0, "no change of caret location on ctrl-p from beginning of line");

  EventUtils.synthesizeKey("e", { ctrlKey: true });
  is(inputNode.selectionStart, 2, "move caret to end of 2 char input with ctrl-e");
  EventUtils.synthesizeKey("e", { ctrlKey: true });
  is(inputNode.selectionStart, 2, "no change of caret location on repeat ctrl-e");
  EventUtils.synthesizeKey("n", { ctrlKey: true });
  is(inputNode.selectionStart, 2, "no change of caret location on ctrl-n from end of line");

  EventUtils.synthesizeKey("p", { ctrlKey: true });
  is(inputNode.selectionStart, 0, "ctrl-p moves to start of line");

  EventUtils.synthesizeKey("n", { ctrlKey: true });
  is(inputNode.selectionStart, 2, "ctrl-n moves to end of line");
}

function testMultiLineInputNavNoHistory() {
  let lineValues = ["one", "2", "something longer", "", "", "three!"];
  jsterm.setInputValue("");
  // simulate shift-return
  for (let i = 0; i < lineValues.length; i++) {
    jsterm.setInputValue(inputNode.value + lineValues[i]);
    EventUtils.synthesizeKey("VK_RETURN", { shiftKey: true });
  }
  let inputValue = inputNode.value;
  is(inputNode.selectionStart, inputNode.selectionEnd);
  is(inputNode.selectionStart, inputValue.length, "caret at end of multiline input");

  // possibility newline is represented by one ('\r', '\n') or two ('\r\n') chars
  let newlineString = inputValue.match(/(\r\n?|\n\r?)$/)[0];

  // Ok, test navigating within the multi-line string!
  EventUtils.synthesizeKey("VK_UP", {});
  let expectedStringAfterCarat = lineValues[5]+newlineString;
  is(inputNode.value.slice(inputNode.selectionStart), expectedStringAfterCarat,
     "up arrow from end of multiline");

  EventUtils.synthesizeKey("VK_DOWN", {});
  is(inputNode.value.slice(inputNode.selectionStart), "",
     "down arrow from within multiline");

  // navigate up through input lines
  EventUtils.synthesizeKey("p", { ctrlKey: true });
  is(inputNode.value.slice(inputNode.selectionStart), expectedStringAfterCarat,
     "ctrl-p from end of multiline");

  for (let i = 4; i >= 0; i--) {
    EventUtils.synthesizeKey("p", { ctrlKey: true });
    expectedStringAfterCarat = lineValues[i] + newlineString + expectedStringAfterCarat;
    is(inputNode.value.slice(inputNode.selectionStart), expectedStringAfterCarat,
       "ctrl-p from within line " + i + " of multiline input");
  }
  EventUtils.synthesizeKey("p", { ctrlKey: true });
  is(inputNode.selectionStart, 0, "reached start of input");
  is(inputNode.value, inputValue,
     "no change to multiline input on ctrl-p from beginning of multiline");

  // navigate to end of first line
  EventUtils.synthesizeKey("e", { ctrlKey: true });
  let caretPos = inputNode.selectionStart;
  let expectedStringBeforeCarat = lineValues[0];
  is(inputNode.value.slice(0, caretPos), expectedStringBeforeCarat,
     "ctrl-e into multiline input");
  EventUtils.synthesizeKey("e", { ctrlKey: true });
  is(inputNode.selectionStart, caretPos,
     "repeat ctrl-e doesn't change caret position in multiline input");

  // navigate down one line; ctrl-a to the beginning; ctrl-e to end
  for (let i = 1; i < lineValues.length; i++) {
    EventUtils.synthesizeKey("n", { ctrlKey: true });
    EventUtils.synthesizeKey("a", { ctrlKey: true });
    caretPos = inputNode.selectionStart;
    expectedStringBeforeCarat += newlineString;
    is(inputNode.value.slice(0, caretPos), expectedStringBeforeCarat,
       "ctrl-a to beginning of line " + (i+1) + " in multiline input");

    EventUtils.synthesizeKey("e", { ctrlKey: true });
    caretPos = inputNode.selectionStart;
    expectedStringBeforeCarat += lineValues[i];
    is(inputNode.value.slice(0, caretPos), expectedStringBeforeCarat,
       "ctrl-e to end of line " + (i+1) + "in multiline input");
  }
}

function testNavWithHistory() {
  // NOTE: Tests does NOT currently define behaviour for ctrl-p/ctrl-n with
  // caret placed _within_ single line input
  let values = ['"single line input"',
                '"a longer single-line input to check caret repositioning"',
                ['"multi-line"', '"input"', '"here!"'].join("\n"),
               ];
  // submit to history
  for (let i = 0; i < values.length; i++) {
    jsterm.setInputValue(values[i]);
    jsterm.execute();
  }
  is(inputNode.selectionStart, 0, "caret location at start of empty line");

  EventUtils.synthesizeKey("p", { ctrlKey: true });
  is(inputNode.selectionStart, values[values.length-1].length,
     "caret location correct at end of last history input");

  // Navigate backwards history with ctrl-p
  for (let i = values.length-1; i > 0; i--) {
    let match = values[i].match(/(\n)/g);
    if (match) {
      // multi-line inputs won't update from history unless caret at beginning
      EventUtils.synthesizeKey("a", { ctrlKey: true });
      for (let i = 0; i < match.length; i++) {
        EventUtils.synthesizeKey("p", { ctrlKey: true });
      }
      EventUtils.synthesizeKey("p", { ctrlKey: true });
    } else {
      // single-line inputs will update from history from end of line
      EventUtils.synthesizeKey("p", { ctrlKey: true });
    }
    is(inputNode.value, values[i-1],
       "ctrl-p updates inputNode from backwards history values[" + i-1 + "]");
  }
  let inputValue = inputNode.value;
  EventUtils.synthesizeKey("p", { ctrlKey: true });
  is(inputNode.selectionStart, 0,
     "ctrl-p at beginning of history moves caret location to beginning of line");
  is(inputNode.value, inputValue,
     "no change to input value on ctrl-p from beginning of line");

  // Navigate forwards history with ctrl-n
  for (let i = 1; i<values.length; i++) {
    EventUtils.synthesizeKey("n", { ctrlKey: true });
    is(inputNode.value, values[i],
       "ctrl-n updates inputNode from forwards history values[" + i + "]");
    is(inputNode.selectionStart, values[i].length,
       "caret location correct at end of history input for values[" + i + "]");
  }
  EventUtils.synthesizeKey("n", { ctrlKey: true });
  ok(!inputNode.value, "ctrl-n at end of history updates to empty input");

  // Simulate editing multi-line
  inputValue = "one\nlinebreak";
  jsterm.setInputValue(inputValue);

  // Attempt nav within input
  EventUtils.synthesizeKey("p", { ctrlKey: true });
  is(inputNode.value, inputValue,
     "ctrl-p from end of multi-line does not trigger history");

  EventUtils.synthesizeKey("a", { ctrlKey: true });
  EventUtils.synthesizeKey("p", { ctrlKey: true });
  is(inputNode.value, values[values.length-1],
     "ctrl-p from start of multi-line triggers history");
}
