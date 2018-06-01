/* -*- indent-tabs-mode: nil; js-indent-level: 2; fill-column: 80 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function testJumpToLine(ed, inputLine, expectCursor) {
  ed.jumpToLine();
  const editorDoc = ed.container.contentDocument;
  const lineInput = editorDoc.querySelector("input");
  lineInput.value = inputLine;
  EventUtils.synthesizeKey("VK_RETURN", { }, editorDoc.defaultView);
  // CodeMirror lines and columns are 0-based, Scratchpad UI is 1-based.
  ch(ed.getCursor(), expectCursor,
    "jumpToLine " + inputLine + " expects cursor " + expectCursor.toSource());
}

function test() {
  waitForExplicitFinish();
  setup((ed, win) => {
    const textLines = [
      "// line 1",
      "//  line 2",
      "//   line 3",
      "//    line 4",
      "//     line 5",
      ""];
    ed.setText(textLines.join("\n"));
    waitForFocus(function() {
      const testVectors = [
        // Various useless inputs go to line 0, column 0 or do nothing.
        ["",
         {line: 0, ch: 0}],
        [":",
         {line: 0, ch: 0}],
        [" ",
         {line: 0, ch: 0}],
        [" : ",
         {line: 0, ch: 0}],
        ["a:b",
         {line: 0, ch: 0}],
        ["LINE: COLUMN ",
         {line: 0, ch: 0}],
        ["-1",
         {line: 0, ch: 0}],
        [":-1",
         {line: 0, ch: 0}],
        ["-1:-1",
         {line: 0, ch: 0}],
        ["0",
         {line: 0, ch: 0}],
        [":0",
         {line: 0, ch: 0}],
        ["0:0",
         {line: 0, ch: 0}],
        // Starting here expect data needs to get updated for length changes to
        // "textLines" above.
        // Just jump to line
        ["1",
         {line: 0, ch: 0}],
        // Jump to second character in line
        ["1:2",
         {line: 0, ch: 1}],
        // Jump to last character on line
        ["1:9",
         {line: 0, ch: 8}],
        // Jump just after last character on line (end of line)
        ["1:10",
         {line: 0, ch: 9}],
        // Jump one character past end of line (gets clamped to end of line)
        ["1:11",
         {line: 0, ch: 9}],
        ["2",
         {line: 1, ch: 0}],
        ["2:2",
         {line: 1, ch: 1}],
        ["2:10",
         {line: 1, ch: 9}],
        ["2:11",
         {line: 1, ch: 10}],
        ["2:12",
         {line: 1, ch: 10}],
        ["3",
         {line: 2, ch: 0}],
        ["3:2",
         {line: 2, ch: 1}],
        ["3:11",
         {line: 2, ch: 10}],
        ["3:12",
         {line: 2, ch: 11}],
        ["3:13",
         {line: 2, ch: 11}],
        ["4",
         {line: 3, ch: 0}],
        ["4:2",
         {line: 3, ch: 1}],
        ["4:12",
         {line: 3, ch: 11}],
        ["4:13",
         {line: 3, ch: 12}],
        ["4:14",
         {line: 3, ch: 12}],
        ["5",
         {line: 4, ch: 0}],
        ["5:2",
         {line: 4, ch: 1}],
        ["5:13",
         {line: 4, ch: 12}],
        ["5:14",
         {line: 4, ch: 13}],
        ["5:15",
         {line: 4, ch: 13}],
        // One line beyond last newline in editor text:
        ["6",
         {line: 5, ch: 0}],
        ["6:2",
         {line: 5, ch: 0}],
        // Two line beyond last newline in editor text (gets clamped):
        ["7",
         {line: 5, ch: 0}],
        ["7:2",
         {line: 5, ch: 0}]
      ];
      testVectors.forEach(vector => {
        testJumpToLine(ed, vector[0], vector[1]);
      });
      teardown(ed, win);
    });
  });
}
