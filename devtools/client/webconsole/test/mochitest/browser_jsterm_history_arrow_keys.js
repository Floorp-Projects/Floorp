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

  const checkInput = (expected, assertionInfo) =>
    checkJsTermValueAndCursor(jsterm, expected, assertionInfo);

  jsterm.focus();
  checkInput("|", "input is empty");

  info("Execute each test value in the console");
  for (const value of TEST_VALUES) {
    jsterm.setInputValue(value);
    await jsterm.execute();
  }

  EventUtils.synthesizeKey("KEY_ArrowUp");
  checkInput("document.location|", "↑: input #4 is correct");
  ok(inputHasNoSelection(jsterm));

  EventUtils.synthesizeKey("KEY_ArrowUp");
  checkInput("document;\nwindow;\ndocument.body|", "↑: input #3 is correct");
  ok(inputHasNoSelection(jsterm));

  info("Move cursor and ensure hitting arrow up twice won't navigate the history");
  EventUtils.synthesizeKey("KEY_ArrowLeft");
  EventUtils.synthesizeKey("KEY_ArrowLeft");
  checkInput("document;\nwindow;\ndocument.bo|dy");

  EventUtils.synthesizeKey("KEY_ArrowUp");
  EventUtils.synthesizeKey("KEY_ArrowUp");

  checkInput("document;|\nwindow;\ndocument.body", "↑↑: input #3 is correct");
  ok(inputHasNoSelection(jsterm));

  EventUtils.synthesizeKey("KEY_ArrowUp");
  checkInput("|document;\nwindow;\ndocument.body", "↑ again: input #3 is correct");
  ok(inputHasNoSelection(jsterm));

  EventUtils.synthesizeKey("KEY_ArrowUp");
  checkInput("document.body|", "↑: input #2 is correct");

  EventUtils.synthesizeKey("KEY_ArrowUp");
  checkInput("window|", "↑: input #1 is correct");

  EventUtils.synthesizeKey("KEY_ArrowUp");
  checkInput("document|", "↑: input #0 is correct");
  ok(inputHasNoSelection(jsterm));

  EventUtils.synthesizeKey("KEY_ArrowDown");
  checkInput("window|", "↓: input #1 is correct");
  ok(inputHasNoSelection(jsterm));

  EventUtils.synthesizeKey("KEY_ArrowDown");
  checkInput("document.body|", "↓: input #2 is correct");

  EventUtils.synthesizeKey("KEY_ArrowDown");
  checkInput("document;\nwindow;\ndocument.body|", "↓: input #3 is correct");
  ok(inputHasNoSelection(jsterm));

  setCursorAtPosition(jsterm, 2);
  checkInput("do|cument;\nwindow;\ndocument.body");

  EventUtils.synthesizeKey("KEY_ArrowDown");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  checkInput("document;\nwindow;\ndo|cument.body", "↓↓: input #3 is correct");
  ok(inputHasNoSelection(jsterm));

  EventUtils.synthesizeKey("KEY_ArrowDown");
  checkInput("document;\nwindow;\ndocument.body|", "↓ again: input #3 is correct");
  ok(inputHasNoSelection(jsterm));

  EventUtils.synthesizeKey("KEY_ArrowDown");
  checkInput("document.location|", "↓: input #4 is correct");

  EventUtils.synthesizeKey("KEY_ArrowDown");
  checkInput("|", "↓: input is empty");
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

function inputHasNoSelection(jsterm) {
  if (jsterm.editor) {
    return !jsterm.editor.getDoc().getSelection();
  }

  return jsterm.inputNode.selectionStart === jsterm.inputNode.selectionEnd;
}
