/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let tempScope = {};
Cu.import("resource:///modules/source-editor.jsm", tempScope);
let SourceEditor = tempScope.SourceEditor;

let editor;
let testWin;

function test()
{
  waitForExplicitFinish();

  const windowUrl = "data:application/vnd.mozilla.xul+xml,<?xml version='1.0'?>" +
    "<window xmlns='http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul'" +
    " title='test for bug 725618 - moveLines shortcut' width='300' height='500'>" +
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

  let text = "target\nfoo\nbar"
  let config = {
    initialText: text,
  };

  editor = new SourceEditor();
  editor.init(box, config, editorLoaded);
}

function editorLoaded()
{
  editor.focus();

  editor.setCaretOffset(0);

  let modifiers = {altKey: true, ctrlKey: Services.appinfo.OS == "Darwin"};

  EventUtils.synthesizeKey("VK_DOWN", modifiers, testWin);
  is(editor.getText(), "foo\ntarget\nbar", "Move lines down works");
  is(editor.getSelectedText(), "target\n", "selection is correct");

  EventUtils.synthesizeKey("VK_DOWN", modifiers, testWin);
  is(editor.getText(), "foo\nbar\ntarget", "Move lines down works");
  is(editor.getSelectedText(), "target", "selection is correct");

  EventUtils.synthesizeKey("VK_DOWN", modifiers, testWin);
  is(editor.getText(), "foo\nbar\ntarget", "Check for bottom of editor works");
  is(editor.getSelectedText(), "target", "selection is correct");

  EventUtils.synthesizeKey("VK_UP", modifiers, testWin);
  is(editor.getText(), "foo\ntarget\nbar", "Move lines up works");
  is(editor.getSelectedText(), "target\n", "selection is correct");

  EventUtils.synthesizeKey("VK_UP", modifiers, testWin);
  is(editor.getText(), "target\nfoo\nbar", "Move lines up works");
  is(editor.getSelectedText(), "target\n", "selection is correct");

  EventUtils.synthesizeKey("VK_UP", modifiers, testWin);
  is(editor.getText(), "target\nfoo\nbar", "Check for top of editor works");
  is(editor.getSelectedText(), "target\n", "selection is correct");

  editor.setSelection(0, 10);
  info("text within selection =" + editor.getSelectedText());

  EventUtils.synthesizeKey("VK_DOWN", modifiers, testWin);
  is(editor.getText(), "bar\ntarget\nfoo", "Multiple line move down works");
  is(editor.getSelectedText(), "target\nfoo", "selection is correct");

  EventUtils.synthesizeKey("VK_DOWN", modifiers, testWin);
  is(editor.getText(), "bar\ntarget\nfoo",
      "Check for bottom of editor works with multiple line selection");
  is(editor.getSelectedText(), "target\nfoo", "selection is correct");

  EventUtils.synthesizeKey("VK_UP", modifiers, testWin);
  is(editor.getText(), "target\nfoo\nbar", "Multiple line move up works");
  is(editor.getSelectedText(), "target\nfoo\n", "selection is correct");

  EventUtils.synthesizeKey("VK_UP", modifiers, testWin);
  is(editor.getText(), "target\nfoo\nbar",
      "Check for top of editor works with multiple line selection");
  is(editor.getSelectedText(), "target\nfoo\n", "selection is correct");

  editor.readOnly = true;

  editor.setCaretOffset(0);

  EventUtils.synthesizeKey("VK_UP", modifiers, testWin);
  is(editor.getText(), "target\nfoo\nbar",
     "Check for readOnly mode works with move lines up");

  EventUtils.synthesizeKey("VK_DOWN", modifiers, testWin);
  is(editor.getText(), "target\nfoo\nbar",
     "Check for readOnly mode works with move lines down");

  finish();
}

registerCleanupFunction(function()
{
  editor.destroy();
  testWin.close();
  testWin = editor = null;
});
