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

  const windowUrl = "data:text/xml,<?xml version='1.0'?>" +
    "<window xmlns='http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul'" +
    " title='test for bug 729960' width='600' height='500'><hbox flex='1'/></window>";
  const windowFeatures = "chrome,titlebar,toolbar,centerscreen,resizable,dialog=no";

  let testWin = Services.ww.openWindow(null, windowUrl, "_blank", windowFeatures, null);
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
    let JSText = "function foo(aVar) {\n" +
                 "  // Block Level 1\n\n" +
                 "  function level2() {\n" +
                 "    let baz = aVar;\n" +
                 "    // Block Level 2\n" +
                 "    function level3() {\n" +
                 "      // Block Level 3\n" +
                 "    }\n" +
                 "  }\n" +
                 "  // Block Level 1" +
                 "  function bar() { /* Block Level 2 */ }\n" +
                 "}";

    editor.setMode(SourceEditor.MODES.JAVASCRIPT);
    editor.setText(JSText);

    // Setting caret at Line 1 bracket start.
    editor.setCaretOffset(19);
    EventUtils.synthesizeKey("]", {accelKey: true}, testWin);
    is(editor.getCaretOffset(), 220,
       "JS : Jump to closing bracket of the code block when caret at block start");

    EventUtils.synthesizeKey("[", {accelKey: true}, testWin);
    is(editor.getCaretOffset(), 19,
       "JS : Jump to opening bracket of the code block when caret at block end");

    // Setting caret at Line 10 start.
    editor.setCaretOffset(161);
    EventUtils.synthesizeKey("[", {accelKey: true}, testWin);
    is(editor.getCaretOffset(), 19,
       "JS : Jump to opening bracket of code block when inside the function");

    editor.setCaretOffset(161);
    EventUtils.synthesizeKey("]", {accelKey: true}, testWin);
    is(editor.getCaretOffset(), 220,
       "JS : Jump to closing bracket of code block when inside the function");

    // Setting caret at Line 6 start.
    editor.setCaretOffset(67);
    EventUtils.synthesizeKey("]", {accelKey: true}, testWin);
    is(editor.getCaretOffset(), 159,
       "JS : Jump to closing bracket in a nested function with caret inside");

    editor.setCaretOffset(67);
    EventUtils.synthesizeKey("[", {accelKey: true}, testWin);
    is(editor.getCaretOffset(), 61,
       "JS : Jump to opening bracket in a nested function with caret inside");

    let CSSText = "#object {\n" +
                  "  property: value;\n" +
                  "  /* comment */\n" +
                  "}";

    editor.setMode(SourceEditor.MODES.CSS);
    editor.setText(CSSText);

    // Setting caret at Line 1 bracket start.
    editor.setCaretOffset(8);
    EventUtils.synthesizeKey("]", {accelKey: true}, testWin);
    is(editor.getCaretOffset(), 45,
       "CSS : Jump to closing bracket of the code block when caret at block start");

    EventUtils.synthesizeKey("[", {accelKey: true}, testWin);
    is(editor.getCaretOffset(), 8,
       "CSS : Jump to opening bracket of the code block when caret at block end");

    // Setting caret at Line 3 start.
    editor.setCaretOffset(28);
    EventUtils.synthesizeKey("[", {accelKey: true}, testWin);
    is(editor.getCaretOffset(), 8,
       "CSS : Jump to opening bracket of code block when inside the function");

    editor.setCaretOffset(28);
    EventUtils.synthesizeKey("]", {accelKey: true}, testWin);
    is(editor.getCaretOffset(), 45,
       "CSS : Jump to closing bracket of code block when inside the function");

    let HTMLText = "<html>\n" +
                   "  <head>\n" +
                   "    <title>Testing Block Jump</title>\n" +
                   "  </head>\n" +
                   "  <body></body>\n" +
                   "</html>";

    editor.setMode(SourceEditor.MODES.HTML);
    editor.setText(HTMLText);

    // Setting caret at Line 1 end.
    editor.setCaretOffset(6);
    EventUtils.synthesizeKey("]", {accelKey: true}, testWin);
    is(editor.getCaretOffset(), 6,
       "HTML : Jump to block end : Nothing happens in html mode");

    // Setting caret at Line 4 end.
    editor.setCaretOffset(64);
    EventUtils.synthesizeKey("[", {accelKey: true}, testWin);
    is(editor.getCaretOffset(), 64,
       "HTML : Jump to block start : Nothing happens in html mode");

    let text = "line 1\n" +
               "line 2\n" +
               "line 3\n" +
               "line 4\n";

    editor.setMode(SourceEditor.MODES.TEXT);
    editor.setText(text);

    // Setting caret at Line 1 start.
    editor.setCaretOffset(0);
    EventUtils.synthesizeKey("]", {accelKey: true}, testWin);
    is(editor.getCaretOffset(), 0,
       "Text : Jump to block end : Nothing happens in text mode");

    // Setting caret at Line 4 end.
    editor.setCaretOffset(28);
    EventUtils.synthesizeKey("[", {accelKey: true}, testWin);
    is(editor.getCaretOffset(), 28,
       "Text : Jump to block start : Nothing happens in text mode");

    editor.destroy();

    testWin.close();
    testWin = editor = null;

    waitForFocus(finish, window);
  }
}
