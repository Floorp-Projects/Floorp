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

  const windowUrl = "data:application/vnd.mozilla.xul+xml,<?xml version='1.0'?>" +
    "<window xmlns='http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul'" +
    " title='test for bug 684546 - reset undo' width='300' height='500'>" +
    "<box flex='1'/></window>";
  const windowFeatures = "chrome,titlebar,toolbar,centerscreen,resizable,dialog=no";

  testWin = Services.ww.openWindow(null, windowUrl, "_blank", windowFeatures, null);
  testWin.addEventListener("load", function onWindowLoad() {
    testWin.removeEventListener("load", onWindowLoad, false);
    waitForFocus(initEditor, testWin);
  }, false);
}

function initEditor()
{
  let box = testWin.document.querySelector("box");

  editor = new SourceEditor();
  editor.init(box, {}, editorLoaded);
}

function editorLoaded()
{
  editor.setText("First");
  editor.setText("Second", 5);
  is(editor.getText(), "FirstSecond", "text set correctly.");
  editor.undo();
  is(editor.getText(), "First", "undo works.");
  editor.redo();
  is(editor.getText(), "FirstSecond", "redo works.");
  editor.resetUndo();
  ok(!editor.canUndo(), "canUndo() is correct");
  ok(!editor.canRedo(), "canRedo() is correct");
  editor.undo();
  is(editor.getText(), "FirstSecond", "reset undo works correctly");
  editor.setText("Third", 11);
  is(editor.getText(), "FirstSecondThird", "text set correctly");
  editor.undo();
  is(editor.getText(), "FirstSecond", "undo works after reset");
  editor.redo();
  is(editor.getText(), "FirstSecondThird", "redo works after reset");
  editor.resetUndo();
  ok(!editor.canUndo(), "canUndo() is correct (again)");
  ok(!editor.canRedo(), "canRedo() is correct (again)");
  editor.undo();
  is(editor.getText(), "FirstSecondThird", "reset undo still works correctly");

  finish();
}

registerCleanupFunction(function() {
  editor.destroy();
  testWin.close();
  testWin = editor = null;
});
