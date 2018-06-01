/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// See Bugs 594497 and 619598.

const TEST_URI = "data:text/html;charset=utf-8,Web Console test for " +
               "bug 594497 and bug 619598";

const TEST_VALUES = [
  "document",
  "window",
  "document.body",
  "document;\nwindow;\ndocument.body",
  "document.location",
];

add_task(async function() {
  // Run test with legacy JsTerm
  await performTests();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTests();
});

async function performTests() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;

  jsterm.focus();
  ok(!jsterm.getInputValue(), "jsterm.getInputValue() is empty");

  info("Execute each test value in the console");
  for (const value of TEST_VALUES) {
    jsterm.setInputValue(value);
    await jsterm.execute();
  }

  const values = TEST_VALUES;

  EventUtils.synthesizeKey("KEY_ArrowUp");

  is(jsterm.getInputValue(), values[4], "VK_UP: jsterm.getInputValue() #4 is correct");
  is(getCaretPosition(jsterm), values[4].length, "caret location is correct");
  ok(inputHasNoSelection(jsterm));

  EventUtils.synthesizeKey("KEY_ArrowUp");

  is(jsterm.getInputValue(), values[3], "VK_UP: jsterm.getInputValue() #3 is correct");
  is(getCaretPosition(jsterm), values[3].length, "caret location is correct");
  ok(inputHasNoSelection(jsterm));

  info("Select text and ensure hitting arrow up twice won't navigate the history");
  setCursorAtPosition(jsterm, values[3].length - 2);
  EventUtils.synthesizeKey("KEY_ArrowUp");
  EventUtils.synthesizeKey("KEY_ArrowUp");

  is(jsterm.getInputValue(), values[3],
    "VK_UP two times: jsterm.getInputValue() #3 is correct");
  is(getCaretPosition(jsterm), jsterm.getInputValue().indexOf("\n"),
    "caret location is correct");
  ok(inputHasNoSelection(jsterm));

  EventUtils.synthesizeKey("KEY_ArrowUp");

  is(jsterm.getInputValue(), values[3],
     "VK_UP again: jsterm.getInputValue() #3 is correct");
  is(getCaretPosition(jsterm), 0, "caret location is correct");
  ok(inputHasNoSelection(jsterm));

  EventUtils.synthesizeKey("KEY_ArrowUp");

  is(jsterm.getInputValue(), values[2], "VK_UP: jsterm.getInputValue() #2 is correct");

  EventUtils.synthesizeKey("KEY_ArrowUp");

  is(jsterm.getInputValue(), values[1], "VK_UP: jsterm.getInputValue() #1 is correct");

  EventUtils.synthesizeKey("KEY_ArrowUp");

  is(jsterm.getInputValue(), values[0], "VK_UP: jsterm.getInputValue() #0 is correct");
  is(getCaretPosition(jsterm), values[0].length, "caret location is correct");
  ok(inputHasNoSelection(jsterm));

  EventUtils.synthesizeKey("KEY_ArrowDown");

  is(jsterm.getInputValue(), values[1], "VK_DOWN: jsterm.getInputValue() #1 is correct");
  is(getCaretPosition(jsterm), values[1].length, "caret location is correct");
  ok(inputHasNoSelection(jsterm));

  EventUtils.synthesizeKey("KEY_ArrowDown");

  is(jsterm.getInputValue(), values[2], "VK_DOWN: jsterm.getInputValue() #2 is correct");

  EventUtils.synthesizeKey("KEY_ArrowDown");

  is(jsterm.getInputValue(), values[3], "VK_DOWN: jsterm.getInputValue() #3 is correct");
  is(getCaretPosition(jsterm), values[3].length, "caret location is correct");
  ok(inputHasNoSelection(jsterm));

  setCursorAtPosition(jsterm, 2);

  EventUtils.synthesizeKey("KEY_ArrowDown");
  EventUtils.synthesizeKey("KEY_ArrowDown");

  is(jsterm.getInputValue(), values[3],
    "VK_DOWN two times: jsterm.getInputValue() #3 is correct");

  ok(getCaretPosition(jsterm) > jsterm.getInputValue().lastIndexOf("\n")
    && inputHasNoSelection(jsterm),
    "caret location is correct");

  EventUtils.synthesizeKey("KEY_ArrowDown");

  is(jsterm.getInputValue(), values[3],
     "VK_DOWN again: jsterm.getInputValue() #3 is correct");
  is(getCaretPosition(jsterm), values[3].length, "caret location is correct");
  ok(inputHasNoSelection(jsterm));

  EventUtils.synthesizeKey("KEY_ArrowDown");

  is(jsterm.getInputValue(), values[4], "VK_DOWN: jsterm.getInputValue() #4 is correct");

  EventUtils.synthesizeKey("KEY_ArrowDown");

  ok(!jsterm.getInputValue(), "VK_DOWN: jsterm.getInputValue() is empty");
}

function setCursorAtPosition(jsterm, pos) {
  const {inputNode, editor} = jsterm;

  if (editor) {
    let line = 0;
    let ch = 0;
    let currentPos = 0;
    jsterm.getInputValue().split("\n").every(l => {
      if (l.length < pos - currentPos) {
        line++;
        currentPos += l.length;
        return true;
      }
      ch = pos - currentPos;
      return false;
    });
    return editor.setCursor({line, ch });
  }

  return inputNode.setSelectionRange(pos, pos);
}

function getCaretPosition(jsterm) {
  const {inputNode, editor} = jsterm;

  if (editor) {
    return editor.getDoc().getRange({line: 0, ch: 0}, editor.getCursor()).length;
  }

  return inputNode.selectionStart;
}

function inputHasNoSelection(jsterm) {
  if (jsterm.editor) {
    return !jsterm.editor.getDoc().getSelection();
  }

  return jsterm.inputNode.selectionStart === jsterm.inputNode.selectionEnd;
}
