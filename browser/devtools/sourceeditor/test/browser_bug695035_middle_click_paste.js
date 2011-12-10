/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource:///modules/source-editor.jsm");

let testWin;
let editor;

function test()
{
  if (Services.appinfo.OS != "Linux") {
    ok(true, "this test only applies to Linux, skipping.")
    return;
  }

  waitForExplicitFinish();

  const windowUrl = "data:text/xml,<?xml version='1.0'?>" +
    "<window xmlns='http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul'" +
    " title='test for bug 695035' width='600' height='500'><hbox flex='1'/></window>";
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
  editor.focus();

  let initialText = "initial text!";

  editor.setText(initialText);

  let expectedString = "foobarBug695035-" + Date.now();

  let doCopy = function() {
    let clipboardHelper = Cc["@mozilla.org/widget/clipboardhelper;1"].
                          getService(Ci.nsIClipboardHelper);
    clipboardHelper.copyStringToClipboard(expectedString,
                                          Ci.nsIClipboard.kSelectionClipboard);
  };

  let onCopy = function() {
    editor.addEventListener(SourceEditor.EVENTS.TEXT_CHANGED, onPaste);

    EventUtils.synthesizeMouse(editor.editorElement, 2, 2, {}, testWin);
    EventUtils.synthesizeMouse(editor.editorElement, 3, 3, {button: 1}, testWin);
  };

  let onPaste = function() {
    editor.removeEventListener(SourceEditor.EVENTS.TEXT_CHANGED, onPaste);

    is(editor.getText(), expectedString + initialText, "middle-click paste works");

    executeSoon(testEnd);
  };

  waitForSelection(expectedString, doCopy, onCopy, testEnd);
}

function testEnd()
{
  editor.destroy();
  testWin.close();

  testWin = editor = null;

  waitForFocus(finish, window);
}
