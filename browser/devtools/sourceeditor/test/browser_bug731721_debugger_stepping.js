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
    " title='test for bug 731721' width='600' height='500'><hbox flex='1'/></window>";
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
    editor.init(hbox, {showAnnotationRuler: true}, editorLoaded);
  }

  function editorLoaded()
  {
    editor.focus();

    editor.setText("line1\nline2\nline3\nline4");

    is(editor.getDebugLocation(), -1, "no debugger location");

    editor.setDebugLocation(1);
    is(editor.getDebugLocation(), 1, "set debugger location works");

    editor.setDebugLocation(3);
    is(editor.getDebugLocation(), 3, "change debugger location works");

    editor.setDebugLocation(-1);
    is(editor.getDebugLocation(), -1, "clear debugger location works");

    editor.destroy();

    testWin.close();
    testWin = editor = null;

    waitForFocus(finish, window);
  }
}
