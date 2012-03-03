/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function test() {

  let temp = {};
  Cu.import("resource:///modules/source-editor.jsm", temp);
  let SourceEditor = temp.SourceEditor;

  let component = Services.prefs.getCharPref(SourceEditor.PREFS.COMPONENT);
  if (component == "textarea") {
    ok(true, "skip test for bug 700893: only applicable for non-textarea components");
    return;
  }

  waitForExplicitFinish();

  let editor;

  const windowUrl = "data:text/xml,<?xml version='1.0'?>" +
    "<window xmlns='http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul'" +
    " title='test for bug 700893' width='600' height='500'><hbox flex='1'/></window>";
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
    editor.init(hbox, {initialText: "foobar"}, editorLoaded);
  }

  function editorLoaded()
  {
    editor.focus();

    is(editor.dirty, false, "editory is not dirty");

    let event = null;
    let eventHandler = function(aEvent) {
      event = aEvent;
    };
    editor.addEventListener(SourceEditor.EVENTS.DIRTY_CHANGED, eventHandler);

    editor.setText("omg");

    is(editor.dirty, true, "editor is dirty");
    ok(event, "DirtyChanged event fired")
    is(event.oldValue, false, "event.oldValue is correct");
    is(event.newValue, true, "event.newValue is correct");

    event = null;
    editor.setText("foo 2");
    ok(!event, "no DirtyChanged event fired");

    editor.dirty = false;

    is(editor.dirty, false, "editor marked as clean");
    ok(event, "DirtyChanged event fired")
    is(event.oldValue, true, "event.oldValue is correct");
    is(event.newValue, false, "event.newValue is correct");

    event = null;
    editor.setText("foo 3");

    is(editor.dirty, true, "editor is dirty after changes");
    ok(event, "DirtyChanged event fired")
    is(event.oldValue, false, "event.oldValue is correct");
    is(event.newValue, true, "event.newValue is correct");

    editor.undo();
    is(editor.dirty, false, "editor is not dirty after undo");
    ok(event, "DirtyChanged event fired")
    is(event.oldValue, true, "event.oldValue is correct");
    is(event.newValue, false, "event.newValue is correct");

    editor.removeEventListener(SourceEditor.EVENTS.DIRTY_CHANGED, eventHandler);

    editor.destroy();

    testWin.close();
    testWin = editor = null;

    waitForFocus(finish, window);
  }
}
