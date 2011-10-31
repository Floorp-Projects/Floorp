/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource:///modules/source-editor.jsm");

let testWin;
let editor;

function test()
{
  let component = Services.prefs.getCharPref(SourceEditor.PREFS.COMPONENT);
  if (component != "orion") {
    ok(true, "skip test for bug 687568: only applicable for Orion");
    return; // Testing for the fix requires direct Orion API access.
  }

  waitForExplicitFinish();

  const windowUrl = "data:application/vnd.mozilla.xul+xml,<?xml version='1.0'?>" +
    "<window xmlns='http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul'" +
    " title='test for bug 687568 - page scroll' width='600' height='500'>" +
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
  editor.init(box, { showLineNumbers: true }, editorLoaded);
}

function editorLoaded()
{
  editor.focus();

  let view = editor._view;
  let model = editor._model;

  let lineHeight = view.getLineHeight();
  let editorHeight = view.getClientArea().height;
  let linesPerPage = Math.floor(editorHeight / lineHeight);
  let totalLines = 3 * linesPerPage;

  let text = "";
  for (let i = 0; i < totalLines; i++) {
    text += "l" + i + "\n";
  }

  editor.setText(text);
  editor.setCaretOffset(0);

  EventUtils.synthesizeKey("VK_DOWN", {shiftKey: true}, testWin);
  EventUtils.synthesizeKey("VK_DOWN", {shiftKey: true}, testWin);
  EventUtils.synthesizeKey("VK_DOWN", {shiftKey: true}, testWin);

  let bottomLine = view.getBottomIndex(true);
  view.setTopIndex(bottomLine + 1);

  executeSoon(function() {
    EventUtils.synthesizeKey("VK_PAGE_DOWN", {shiftKey: true}, testWin);

    executeSoon(function() {
      let topLine = view.getTopIndex(true);
      let topLineOffset = model.getLineStart(topLine);
      let selection = editor.getSelection();
      ok(selection.start < topLineOffset && topLineOffset < selection.end,
         "top visible line is selected");

      editor.destroy();
      testWin.close();
      testWin = editor = null;

      waitForFocus(finish, window);
    });
  });
}
