/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let tempScope = {};
Cu.import("resource:///modules/source-editor.jsm", tempScope);
let SourceEditor = tempScope.SourceEditor;

let testWin;
let editor;
const VERTICAL_OFFSET = 3;

function test()
{
  waitForExplicitFinish();

  const windowUrl = "data:application/vnd.mozilla.xul+xml,<?xml version='1.0'?>" +
    "<window xmlns='http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul'" +
    " title='test for bug 729480 - allow setCaretPosition align the target line" +
    " vertically in view according to a third argument'" +
    " width='300' height='300'><box flex='1'/></window>";
  const windowFeatures = "chrome,titlebar,toolbar,centerscreen,dialog=no";

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
  editor.init(box, {showLineNumbers: true}, editorLoaded);
}

function editorLoaded()
{
  editor.focus();

  // setting 3 pages of lines containing the line number.
  let view = editor._view;

  let lineHeight = view.getLineHeight();
  let editorHeight = view.getClientArea().height;
  let linesPerPage = Math.floor(editorHeight / lineHeight);
  let totalLines = 3 * linesPerPage;

  let text = "";
  for (let i = 0; i < totalLines; i++) {
    text += "Line " + i + "\n";
  }

  editor.setText(text);
  editor.setCaretOffset(0);

  let offset = Math.min(Math.round(linesPerPage/2), VERTICAL_OFFSET);
  // Building the iterator array.
  // [line, alignment, topIndex_check]
  let iterateOn = [
    [0, "TOP", 0],
    [25, "TOP", 25 - offset],
    // Case when the target line is already in view.
    [27, "TOP", 25 - offset],
    [0, "BOTTOM", 0],
    [5, "BOTTOM", 0],
    [38, "BOTTOM", 38 - linesPerPage + offset],
    [0, "CENTER", 0],
    [4, "CENTER", 0],
    [34, "CENTER", 34 - Math.round(linesPerPage/2)]
  ];

  function testEnd() {
    editor.destroy();
    testWin.close();
    testWin = editor = null;
    waitForFocus(finish, window);
  }

  function testPosition(pos) {
    is(editor.getTopIndex(), iterateOn[pos][2], "scroll is correct for test #" + pos);
    iterator(++pos);
  }

  function iterator(i) {
    if (i == iterateOn.length) {
      testEnd();
    } else {
      editor.setCaretPosition(iterateOn[i][0], 0,
                              editor.VERTICAL_ALIGN[iterateOn[i][1]]);
      executeSoon(testPosition.bind(this, i));
    }
  }
  iterator(0);
}
