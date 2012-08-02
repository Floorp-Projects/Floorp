/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let tempScope = {};
Cu.import("resource:///modules/source-editor.jsm", tempScope);
let SourceEditor = tempScope.SourceEditor;

let testWin;
let editor;

function test()
{
  waitForExplicitFinish();

  const windowUrl = "data:text/xml,<?xml version='1.0'?>" +
    "<window xmlns='http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul'" +
    " title='test for bug 660784' width='600' height='500'><hbox flex='1'/></window>";
  const windowFeatures = "chrome,titlebar,toolbar,centerscreen,resizable,dialog=no";

  testWin = Services.ww.openWindow(null, windowUrl, "_blank", windowFeatures, null);
  testWin.addEventListener("load", function onWindowLoad() {
    testWin.removeEventListener("load", onWindowLoad, false);
    waitForFocus(initEditor, testWin);
  }, false);
}

function initEditor()
{
  let hbox = testWin.document.querySelector("hbox");
  editor = new SourceEditor();
  editor.init(hbox, {}, editorLoaded);
}

function editorLoaded()
{
  let component = Services.prefs.getCharPref(SourceEditor.PREFS.COMPONENT);

  editor.focus();

  editor.setText("line1\nline2\nline3");

  if (component != "textarea") {
    is(editor.getLineCount(), 3, "getLineCount() works");
  }

  editor.setCaretPosition(1);
  is(editor.getCaretOffset(), 6, "setCaretPosition(line) works");

  let pos;
  if (component != "textarea") {
    pos = editor.getCaretPosition();
    ok(pos.line == 1 && pos.col == 0, "getCaretPosition() works");
  }

  editor.setCaretPosition(1, 3);
  is(editor.getCaretOffset(), 9, "setCaretPosition(line, column) works");

  if (component != "textarea") {
    pos = editor.getCaretPosition();
    ok(pos.line == 1 && pos.col == 3, "getCaretPosition() works");
  }

  editor.setCaretPosition(2);
  is(editor.getCaretOffset(), 12, "setCaretLine() works, confirmed");

  if (component != "textarea") {
    pos = editor.getCaretPosition();
    ok(pos.line == 2 && pos.col == 0, "setCaretPosition(line) works, again");
  }

  let offsetLine = editor.getLineAtOffset(0);
  is(offsetLine, 0, "getLineAtOffset() is correct for offset 0");

  let offsetLine = editor.getLineAtOffset(6);
  is(offsetLine, 1, "getLineAtOffset() is correct for offset 6");

  let offsetLine = editor.getLineAtOffset(12);
  is(offsetLine, 2, "getLineAtOffset() is correct for offset 12");

  editor.destroy();

  testWin.close();
  testWin = editor = null;

  waitForFocus(finish, window);
}

