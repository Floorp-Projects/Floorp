/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function test() {

  let temp = {};
  Cu.import("resource:///modules/source-editor.jsm", temp);
  let SourceEditor = temp.SourceEditor;

  let component = Services.prefs.getCharPref(SourceEditor.PREFS.COMPONENT);
  if (component == "textarea") {
    ok(true, "skip test for bug 712982: only applicable for non-textarea components");
    return;
  }

  waitForExplicitFinish();

  let editor;

  const windowUrl = "data:text/xml,<?xml version='1.0'?>" +
    "<window xmlns='http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul'" +
    " title='test for bug 712982' width='600' height='500'><hbox flex='1'/></window>";
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

    editor.setText("line1\nline2\nline3\nline4");

    editor.setCaretPosition(3);
    let pos = editor.getCaretPosition();
    ok(pos.line == 3 && pos.col == 0, "initial caret location is correct");

    EventUtils.synthesizeMouse(editor.editorElement, 10, 10, {}, testWin);

    is(editor.getCaretOffset(), 0, "click on line 0 worked");

    editor.setCaretPosition(2);
    EventUtils.synthesizeMouse(editor.editorElement, 11, 11,
                               {shiftKey: true}, testWin);
    is(editor.getSelectedText().trim(), "line1\nline2", "shift+click works");

    editor.setCaretOffset(0);

    EventUtils.synthesizeMouse(editor.editorElement, 10, 10,
                               {clickCount: 2}, testWin);

    is(editor.getSelectedText().trim(), "line1", "double click works");

    editor.destroy();

    testWin.close();
    testWin = editor = null;

    waitForFocus(finish, window);
  }
}
