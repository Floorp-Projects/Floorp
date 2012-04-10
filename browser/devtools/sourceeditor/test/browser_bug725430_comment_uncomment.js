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
    " title='test for bug 725430' width='600' height='500'><hbox flex='1'/></window>";
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
    let text = "firstline\nsecondline\nthirdline\nfourthline";

    editor.setMode(SourceEditor.MODES.JAVASCRIPT);
    editor.setText(text)

    editor.setCaretPosition(0);
    EventUtils.synthesizeKey("/", {accelKey: true}, testWin);
    is(editor.getText(), "//" + text, "JS Single line Commenting Works");
    editor.undo();
    is(editor.getText(), text, "Undo Single Line Commenting action works");
    editor.redo();
    is(editor.getText(), "//" + text, "Redo works");
    editor.setCaretPosition(0);
    EventUtils.synthesizeKey("/", {accelKey: true}, testWin);
    is(editor.getText(), text, "JS Single Line Uncommenting works");

    editor.setText(text);

    EventUtils.synthesizeKey("VK_A", {accelKey: true}, testWin);
    EventUtils.synthesizeKey("/", {accelKey: true}, testWin);
    is(editor.getText(), "/*" + text + "*/", "JS Block Commenting works");
    editor.undo();
    is(editor.getText(), text, "Undo Block Commenting action works");
    editor.redo();
    is(editor.getText(), "/*" + text + "*/", "Redo works");
    EventUtils.synthesizeKey("VK_A", {accelKey: true}, testWin);
    EventUtils.synthesizeKey("/", {accelKey: true}, testWin);
    is(editor.getText(), text, "JS Block Uncommenting works");
    editor.undo();
    is(editor.getText(), "/*" + text + "*/", "Undo Block Uncommenting works");
    editor.redo();
    is(editor.getText(), text, "Redo works");

    let regText = "//firstline\n    //    secondline\nthird//line\n//fourthline";
    let expText = "firstline\n        secondline\nthird//line\nfourthline";
    editor.setText(regText);
    EventUtils.synthesizeKey("VK_A", {accelKey: true}, testWin);
    EventUtils.synthesizeKey("/", {accelKey: true}, testWin);
    is(editor.getText(), expText, "JS Multiple Line Uncommenting works");
    editor.undo();
    is(editor.getText(), regText, "Undo Multiple Line Uncommenting works");
    editor.redo();
    is(editor.getText(), expText, "Redo works");

    editor.setMode(SourceEditor.MODES.CSS);
    editor.setText(text);

    expText = "/*firstline*/\nsecondline\nthirdline\nfourthline";
    editor.setCaretPosition(0);
    EventUtils.synthesizeKey("/", {accelKey: true}, testWin);
    is(editor.getText(), expText, "CSS Commenting without selection works");
    editor.setCaretPosition(0);
    EventUtils.synthesizeKey("/", {accelKey: true}, testWin);
    is(editor.getText(), text, "CSS Uncommenting without selection works");

    editor.setText(text);

    EventUtils.synthesizeKey("VK_A", {accelKey: true}, testWin);
    EventUtils.synthesizeKey("/", {accelKey: true}, testWin);
    is(editor.getText(), "/*" + text + "*/", "CSS Multiple Line Commenting works");
    EventUtils.synthesizeKey("VK_A", {accelKey: true}, testWin);
    EventUtils.synthesizeKey("/", {accelKey: true}, testWin);
    is(editor.getText(), text, "CSS Multiple Line Uncommenting works");

    editor.setMode(SourceEditor.MODES.HTML);
    editor.setText(text);

    expText = "<!--firstline-->\nsecondline\nthirdline\nfourthline";
    editor.setCaretPosition(0);
    EventUtils.synthesizeKey("/", {accelKey: true}, testWin);
    is(editor.getText(), expText, "HTML Commenting without selection works");
    editor.setCaretPosition(0);
    EventUtils.synthesizeKey("/", {accelKey: true}, testWin);
    is(editor.getText(), text, "HTML Uncommenting without selection works");

    editor.setText(text);

    EventUtils.synthesizeKey("VK_A", {accelKey: true}, testWin);
    EventUtils.synthesizeKey("/", {accelKey: true}, testWin);
    is(editor.getText(), "<!--" + text + "-->", "HTML Multiple Line Commenting works");
    EventUtils.synthesizeKey("VK_A", {accelKey: true}, testWin);
    EventUtils.synthesizeKey("/", {accelKey: true}, testWin);
    is(editor.getText(), text, "HTML Multiple Line Uncommenting works");

    editor.setMode(SourceEditor.MODES.TEXT);
    editor.setText(text);

    editor.setCaretPosition(0);
    EventUtils.synthesizeKey("/", {accelKey: true}, testWin);
    is(editor.getText(), text, "Commenting disabled in Text mode");
    editor.setText(regText);
    EventUtils.synthesizeKey("VK_A", {accelKey: true}, testWin);
    EventUtils.synthesizeKey("/", {accelKey: true}, testWin);
    is(editor.getText(), regText, "Uncommenting disabled in Text mode");

    editor.setText(text);
    editor.readOnly = true;

    editor.setCaretPosition(0);
    EventUtils.synthesizeKey("/", {accelKey: true}, testWin);
    is(editor.getText(), text, "Commenting disabled in ReadOnly mode");
    editor.setText(regText);
    EventUtils.synthesizeKey("VK_A", {accelKey: true}, testWin);
    EventUtils.synthesizeKey("/", {accelKey: true}, testWin);
    is(editor.getText(), regText, "Uncommenting disabled in ReadOnly mode");

    editor.destroy();

    testWin.close();
    testWin = editor = null;

    waitForFocus(finish, window);
  }
}
