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
                                          Ci.nsIClipboard.kSelectionClipboard,
                                          testWin.document);
  };

  let onCopy = function() {
    editor.addEventListener(SourceEditor.EVENTS.TEXT_CHANGED, onPaste);

    EventUtils.synthesizeMouse(editor.editorElement, 10, 10, {}, testWin);
    EventUtils.synthesizeMouse(editor.editorElement, 11, 11, {button: 1}, testWin);
  };

  let onPaste = function() {
    editor.removeEventListener(SourceEditor.EVENTS.TEXT_CHANGED, onPaste);

    let text = editor.getText();
    isnot(text.indexOf(expectedString), -1, "middle-click paste works");
    isnot(text, initialText, "middle-click paste works (confirmed)");

    executeSoon(doTestBug695032);
  };

  let doTestBug695032 = function() {
    info("test for bug 695032 - editor selection should be placed in the X11 primary selection buffer");

    let text = "foobarBug695032 test me, test me!";
    editor.setText(text);

    waitForSelection(text, function() {
      EventUtils.synthesizeKey("a", {accelKey: true}, testWin);
    }, testEnd, testEnd);
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
