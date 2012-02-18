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
    ok(true, "skip test for bug 707987: only applicable for non-textarea components");
    return;
  }

  waitForExplicitFinish();

  let editor;

  const windowUrl = "data:text/xml,<?xml version='1.0'?>" +
    "<window xmlns='http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul'" +
    " title='test for bug 707987' width='600' height='500'><hbox flex='1'/></window>";
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
    editor.init(hbox, {showAnnotationRuler: true}, editorLoaded);
  }

  function editorLoaded()
  {
    editor.focus();

    editor.setText("line1\nline2\nline3\nline4");

    is(editor.getBreakpoints().length, 0, "no breakpoints");

    let event = null;
    let eventHandler = function(aEvent) {
      event = aEvent;
    };
    editor.addEventListener(SourceEditor.EVENTS.BREAKPOINT_CHANGE, eventHandler);

    // Add breakpoint at line 0

    editor.addBreakpoint(0);

    let breakpoints = editor.getBreakpoints();
    is(breakpoints.length, 1, "one breakpoint added");
    is(breakpoints[0].line, 0, "breakpoint[0].line is correct");
    ok(!breakpoints[0].condition, "breakpoint[0].condition is correct");

    ok(event, "breakpoint event fired");
    is(event.added.length, 1, "one breakpoint added (confirmed)");
    is(event.removed.length, 0, "no breakpoint removed");
    is(event.added[0].line, 0, "event added[0].line is correct");
    ok(!event.added[0].condition, "event added[0].condition is correct");

    // Add breakpoint at line 3

    event = null;
    editor.addBreakpoint(3, "foo == 'bar'");

    breakpoints = editor.getBreakpoints();
    is(breakpoints.length, 2, "another breakpoint added");
    is(breakpoints[0].line, 0, "breakpoint[0].line is correct");
    ok(!breakpoints[0].condition, "breakpoint[0].condition is correct");
    is(breakpoints[1].line, 3, "breakpoint[1].line is correct");
    is(breakpoints[1].condition, "foo == 'bar'",
       "breakpoint[1].condition is correct");

    ok(event, "breakpoint event fired");
    is(event.added.length, 1, "another breakpoint added (confirmed)");
    is(event.removed.length, 0, "no breakpoint removed");
    is(event.added[0].line, 3, "event added[0].line is correct");
    is(event.added[0].condition, "foo == 'bar'",
       "event added[0].condition is correct");

    // Try to add another breakpoint at line 0

    event = null;
    editor.addBreakpoint(0);

    is(editor.getBreakpoints().length, 2, "no breakpoint added");
    is(event, null, "no breakpoint event fired");

    // Try to remove a breakpoint from line 1

    is(editor.removeBreakpoint(1), false, "removeBreakpoint(1) returns false");
    is(editor.getBreakpoints().length, 2, "no breakpoint removed");
    is(event, null, "no breakpoint event fired");

    // Remove the breakpoint from line 0

    is(editor.removeBreakpoint(0), true, "removeBreakpoint(0) returns true");

    breakpoints = editor.getBreakpoints();
    is(breakpoints[0].line, 3, "breakpoint[0].line is correct");
    is(breakpoints[0].condition, "foo == 'bar'",
       "breakpoint[0].condition is correct");

    ok(event, "breakpoint event fired");
    is(event.added.length, 0, "no breakpoint added");
    is(event.removed.length, 1, "one breakpoint removed");
    is(event.removed[0].line, 0, "event removed[0].line is correct");
    ok(!event.removed[0].condition, "event removed[0].condition is correct");

    // Remove the breakpoint from line 3

    event = null;
    is(editor.removeBreakpoint(3), true, "removeBreakpoint(3) returns true");

    is(editor.getBreakpoints().length, 0, "no breakpoints");
    ok(event, "breakpoint event fired");
    is(event.added.length, 0, "no breakpoint added");
    is(event.removed.length, 1, "one breakpoint removed");
    is(event.removed[0].line, 3, "event removed[0].line is correct");
    is(event.removed[0].condition, "foo == 'bar'",
       "event removed[0].condition is correct");

    // Add a breakpoint with the mouse

    event = null;
    EventUtils.synthesizeMouse(editor.editorElement, 10, 10, {}, testWin);

    breakpoints = editor.getBreakpoints();
    is(breakpoints.length, 1, "one breakpoint added");
    is(breakpoints[0].line, 0, "breakpoint[0].line is correct");
    ok(!breakpoints[0].condition, "breakpoint[0].condition is correct");

    ok(event, "breakpoint event fired");
    is(event.added.length, 1, "one breakpoint added (confirmed)");
    is(event.removed.length, 0, "no breakpoint removed");
    is(event.added[0].line, 0, "event added[0].line is correct");
    ok(!event.added[0].condition, "event added[0].condition is correct");

    // Remove a breakpoint with the mouse

    event = null;
    EventUtils.synthesizeMouse(editor.editorElement, 10, 10, {}, testWin);

    breakpoints = editor.getBreakpoints();
    is(breakpoints.length, 0, "one breakpoint removed");

    ok(event, "breakpoint event fired");
    is(event.added.length, 0, "no breakpoint added");
    is(event.removed.length, 1, "one breakpoint removed (confirmed)");
    is(event.removed[0].line, 0, "event removed[0].line is correct");
    ok(!event.removed[0].condition, "event removed[0].condition is correct");

    editor.destroy();

    testWin.close();
    testWin = editor = null;

    waitForFocus(finish, window);
  }
}
