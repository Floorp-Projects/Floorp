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
    " title='Test for bug 703692' width='600' height='500'><hbox flex='1'/></window>";
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
  let focusHandler = function(aEvent) {
    editor.removeEventListener(SourceEditor.EVENTS.FOCUS, focusHandler);
    editor.addEventListener(SourceEditor.EVENTS.BLUR, blurHandler);

    ok(aEvent, "Focus event fired");
    window.focus();
  };

  let blurHandler = function(aEvent) {
    editor.removeEventListener(SourceEditor.EVENTS.BLUR, blurHandler);

    ok(aEvent, "Blur event fired");
    executeSoon(testEnd);
  }

  editor.addEventListener(SourceEditor.EVENTS.FOCUS, focusHandler);

  editor.focus();
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
