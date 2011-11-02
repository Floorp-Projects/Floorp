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
    ok(true, "skip test for bug 687580: only applicable for Orion");
    return; // Testing for the fix requires direct Orion API access.
  }

  waitForExplicitFinish();

  const windowUrl = "data:application/vnd.mozilla.xul+xml,<?xml version='1.0'?>" +
    "<window xmlns='http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul'" +
    " title='test for bug 687580 - drag and drop' width='600' height='500'>" +
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
  editor.focus();

  let view = editor._view;
  let model = editor._model;

  let lineHeight = view.getLineHeight();
  let editorHeight = view.getClientArea().height;
  let linesPerPage = Math.floor(editorHeight / lineHeight);
  let totalLines = 2 * linesPerPage;

  let text = "foobarBug687580-";
  for (let i = 0; i < totalLines; i++) {
    text += "l" + i + "\n";
  }

  editor.setText(text);
  editor.setCaretOffset(0);

  let bottomPixel = view.getBottomPixel();

  EventUtils.synthesizeKey("VK_DOWN", {shiftKey: true}, testWin);
  EventUtils.synthesizeKey("VK_DOWN", {shiftKey: true}, testWin);
  EventUtils.synthesizeKey("VK_DOWN", {shiftKey: true}, testWin);

  let initialSelection = editor.getSelection();

  let ds = Cc["@mozilla.org/widget/dragservice;1"].
           getService(Ci.nsIDragService);

  let target = view._dragNode;
  let targetWin = target.ownerDocument.defaultView;

  let dataTransfer = null;

  let onDragStart = function(aEvent) {
    target.removeEventListener("dragstart", onDragStart, false);

    dataTransfer = aEvent.dataTransfer;
    ok(dataTransfer, "dragstart event fired");
    ok(dataTransfer.types.contains("text/plain"),
       "dataTransfer text/plain available");
    let text = dataTransfer.getData("text/plain");
    isnot(text.indexOf("foobarBug687580"), -1, "text/plain data is correct");

    dataTransfer.dropEffect = "move";
  };

  let onDrop = executeSoon.bind(null, function() {
    target.removeEventListener("drop", onDrop, false);

    let selection = editor.getSelection();
    is(selection.start, selection.end, "selection is collapsed");
    is(editor.getText(0, 2), "l3", "drag and drop worked");

    let offset = editor.getCaretOffset();
    ok(offset > initialSelection.end, "new caret location");

    let initialLength = initialSelection.end - initialSelection.start;
    let dropText = editor.getText(offset - initialLength, offset);
    isnot(dropText.indexOf("foobarBug687580"), -1, "drop text is correct");

    editor.destroy();
    testWin.close();
    testWin = editor = null;

    waitForFocus(finish, window);
  });

  executeSoon(function() {
    ds.startDragSession();

    target.addEventListener("dragstart", onDragStart, false);
    target.addEventListener("drop", onDrop, false);

    EventUtils.synthesizeMouse(target, 10, 10, {type: "mousedown"}, targetWin);

    EventUtils.synthesizeMouse(target, 11, bottomPixel - 25, {type: "mousemove"},
                               targetWin);

    EventUtils.synthesizeMouse(target, 12, bottomPixel - 15, {type: "mousemove"},
                               targetWin);

    let clientX = 5;
    let clientY = bottomPixel - 10;

    let event = targetWin.document.createEvent("DragEvents");
    event.initDragEvent("dragenter", true, true, targetWin, 0, 0, 0, clientX,
                        clientY, false, false, false, false, 0, null,
                        dataTransfer);
    target.dispatchEvent(event);

    event = targetWin.document.createEvent("DragEvents");
    event.initDragEvent("dragover", true, true, targetWin, 0, 0, 0, clientX + 1,
                        clientY + 2, false, false, false, false, 0, null,
                        dataTransfer);
    target.dispatchEvent(event);

    EventUtils.synthesizeMouse(target, clientX + 2, clientY + 1,
                               {type: "mouseup"}, targetWin);

    event = targetWin.document.createEvent("DragEvents");
    event.initDragEvent("drop", true, true, targetWin, 0, 0, 0, clientX + 2,
                        clientY + 3, false, false, false, false, 0, null,
                        dataTransfer);
    target.dispatchEvent(event);

    event = targetWin.document.createEvent("DragEvents");
    event.initDragEvent("dragend", true, true, targetWin, 0, 0, 0, clientX + 3,
                        clientY + 2, false, false, false, false, 0, null,
                        dataTransfer);
    target.dispatchEvent(event);

    ds.endDragSession(true);
  });
}
