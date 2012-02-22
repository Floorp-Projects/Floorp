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

  const windowUrl = "data:text/xml,<?xml version='1.0'?>" +
    "<window xmlns='http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul'" +
    " title='Test for bug 725388' width='600' height='500'><hbox flex='1'/></window>";
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
  testWin.resizeBy(1, 2);

  let text = "BrowserBug - 725388";
  editor.setText(text);

  let target = editor.editorElement;
  let targetWin = target.ownerDocument.defaultView;

  let eventsFired = 0;

  let done = function() {
    eventsFired++;
    if (eventsFired == 3) {
      executeSoon(testEnd);
    }
  };

  let mMoveHandler = function(aEvent) {
    editor.removeEventListener(SourceEditor.EVENTS.MOUSE_MOVE, mMoveHandler);

    is(aEvent.event.type, "mousemove", "MouseMove event fired.");

    executeSoon(done);
  };

  let mOverHandler = function(aEvent) {
    editor.removeEventListener(SourceEditor.EVENTS.MOUSE_OVER, mOverHandler);

    is(aEvent.event.type, "mouseover", "MouseOver event fired.");

    executeSoon(done);
  };

  let mOutHandler = function(aEvent) {
    editor.removeEventListener(SourceEditor.EVENTS.MOUSE_OUT, mOutHandler);

    is(aEvent.event.type, "mouseout", "MouseOut event fired.");

    executeSoon(done);
  };

  editor.addEventListener(SourceEditor.EVENTS.MOUSE_OVER, mOverHandler);
  editor.addEventListener(SourceEditor.EVENTS.MOUSE_MOVE, mMoveHandler);
  editor.addEventListener(SourceEditor.EVENTS.MOUSE_OUT, mOutHandler);

  waitForFocus(function() {
    EventUtils.synthesizeMouse(target, 10, 10, {type: "mouseover"},
                               targetWin);
    EventUtils.synthesizeMouse(target, 15, 17, {type: "mousemove"},
                               targetWin);
    EventUtils.synthesizeMouse(target, -10, -10, {type: "mouseout"},
                               targetWin);
  }, targetWin);
}

function testEnd()
{
  if (editor) {
    editor.destroy();
  }
  if (testWin) {
    testWin.close();
  }
  testWin = editor = null;

  waitForFocus(finish, window);
}
