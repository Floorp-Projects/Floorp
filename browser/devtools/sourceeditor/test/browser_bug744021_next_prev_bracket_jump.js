/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function test() {

  let temp = {};
  Cu.import("resource:///modules/source-editor.jsm", temp);
  let SourceEditor = temp.SourceEditor;

  waitForExplicitFinish();

  let editor;

  const windowUrl = "data:text/xml;charset=utf8,<?xml version='1.0'?><window " +
    "xmlns='http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul' " +
    "title='test for bug 744021' width='600' height='500'><hbox flex='1'/>" +
    "</window>";
  const windowFeatures = "chrome,titlebar,toolbar,centerscreen,resizable," +
    "dialog=no";

  let testWin = Services.ww.openWindow(null, windowUrl, "_blank",
                                       windowFeatures, null);
  testWin.addEventListener("load", function onWindowLoad() {
    testWin.removeEventListener("load", onWindowLoad, false);
    waitForFocus(initEditor, testWin);
  }, false);

  function initEditor()
  {
    let hbox = testWin.document.querySelector("hbox");
    editor = new SourceEditor();
    editor.init(hbox, {showLineNumbers: true}, editorLoaded);
  }

  function editorLoaded()
  {
    editor.focus();
    let JSText = "function foo() {\n" +
                 "  \n" +
                 "  function level2() {\n" +
                 "    \n" +
                 "    function level3() {\n" +
                 "      \n" +
                 "    }\n" +
                 "  }\n" +
                 "  function bar() { /* Block Level 2 */ }\n" +
                 "}\n" +
                 "function baz() {\n" +
                 "  \n" +
                 "}";

    editor.setMode(SourceEditor.MODES.JAVASCRIPT);
    editor.setText(JSText);

    // Setting caret at end of line 11 (function baz() {).
    editor.setCaretOffset(147);
    EventUtils.synthesizeKey("[", {accelKey: true, shiftKey: true}, testWin);
    is(editor.getCaretOffset(), 16,
       "JS : Jump to opening bracket of previous sibling block when no parent");

    EventUtils.synthesizeKey("]", {accelKey: true, shiftKey: true}, testWin);
    is(editor.getCaretOffset(), 129,
       "JS : Jump to closing bracket of same code block");

    EventUtils.synthesizeKey("]", {accelKey: true, shiftKey: true}, testWin);
    is(editor.getCaretOffset(), 151,
       "JS : Jump to closing bracket of next sibling code block");

    let CSSText = "#object1 {\n" +
                  "  property: value;\n" +
                  "  /* comment */\n" +
                  "}\n" +
                  ".class1 {\n" +
                  "  property: value;\n" +
                  "}";

    editor.setMode(SourceEditor.MODES.CSS);
    editor.setText(CSSText);

    // Setting caret at Line 5 end (.class1 {).
    editor.setCaretOffset(57);
    EventUtils.synthesizeKey("[", {accelKey: true, shiftKey: true}, testWin);
    is(editor.getCaretOffset(), 10,
       "CSS : Jump to opening bracket of previous sibling code block");

    EventUtils.synthesizeKey("]", {accelKey: true, shiftKey: true}, testWin);
    is(editor.getCaretOffset(), 46,
       "CSS : Jump to closing bracket of same code block");

    EventUtils.synthesizeKey("]", {accelKey: true, shiftKey: true}, testWin);
    is(editor.getCaretOffset(), 77,
       "CSS : Jump to closing bracket of next sibling code block");

    editor.destroy();

    testWin.close();
    testWin = editor = null;

    waitForFocus(finish, window);
  }
}
