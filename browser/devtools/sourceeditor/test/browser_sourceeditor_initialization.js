/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let tempScope = {};
Cu.import("resource:///modules/source-editor.jsm", tempScope);
let SourceEditor = tempScope.SourceEditor;

let testWin;
let testDoc;
let editor;

function test()
{
  waitForExplicitFinish();

  const windowUrl = "data:text/xml,<?xml version='1.0'?>" +
    "<window xmlns='http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul'" +
    " title='test for bug 660784' width='600' height='500'><hbox flex='1'/></window>";
  const windowFeatures = "chrome,titlebar,toolbar,centerscreen,resizable,dialog=no";

  testWin = Services.ww.openWindow(null, windowUrl, "_blank", windowFeatures, null);
  testWin.addEventListener("load", function onWindowLoad() {
    testWin.removeEventListener("load", onWindowLoad, false);
    waitForFocus(initEditor, testWin);
  }, false);
}

function initEditor()
{
  testDoc = testWin.document;

  let hbox = testDoc.querySelector("hbox");

  editor = new SourceEditor();
  let config = {
    showLineNumbers: true,
    initialText: "foobarbaz",
    tabSize: 7,
    expandTab: true,
  };

  editor.init(hbox, config, editorLoaded);
}

function editorLoaded()
{
  ok(editor.editorElement, "editor loaded");

  is(editor.parentElement, testDoc.querySelector("hbox"),
     "parentElement is correct");

  editor.focus();

  is(editor.getMode(), SourceEditor.DEFAULTS.mode, "default editor mode");

  // Test general editing methods.

  ok(!editor.canUndo(), "canUndo() works (nothing to undo), just loaded");

  ok(!editor.readOnly, "editor is not read-only");

  is(editor.getText(), "foobarbaz", "placeholderText works");

  is(editor.getText().length, editor.getCharCount(),
     "getCharCount() is correct");

  is(editor.getText(3, 5), "ba", "getText() range works");

  editor.setText("source-editor");

  is(editor.getText(), "source-editor", "setText() works");

  editor.setText("code", 0, 6);

  is(editor.getText(), "code-editor", "setText() range works");

  ok(editor.canUndo(), "canUndo() works (things to undo)");
  ok(!editor.canRedo(), "canRedo() works (nothing to redo yet)");

  editor.undo();

  is(editor.getText(), "source-editor", "undo() works");

  ok(editor.canRedo(), "canRedo() works (things to redo)");

  editor.redo();

  is(editor.getText(), "code-editor", "redo() works");

  EventUtils.synthesizeKey("VK_Z", {accelKey: true}, testWin);

  is(editor.getText(), "source-editor", "Ctrl-Z (undo) works");

  EventUtils.synthesizeKey("VK_Z", {accelKey: true, shiftKey: true}, testWin);

  is(editor.getText(), "code-editor", "Ctrl-Shift-Z (redo) works");

  // Test selection methods.

  editor.setSelection(0, 4);

  is(editor.getSelectedText(), "code", "getSelectedText() works");

  let selection = editor.getSelection();
  ok(selection.start == 0 && selection.end == 4, "getSelection() works");

  editor.dropSelection();

  selection = editor.getSelection();
  ok(selection.start == 4 && selection.end == 4, "dropSelection() works");

  editor.setCaretOffset(7);
  is(editor.getCaretOffset(), 7, "setCaretOffset() works");

  // Test grouped changes.

  editor.setText("foobar");

  editor.startCompoundChange();

  editor.setText("foo1");
  editor.setText("foo2");
  editor.setText("foo3");

  editor.endCompoundChange();

  is(editor.getText(), "foo3", "editor content is correct");

  editor.undo();
  is(editor.getText(), "foobar", "compound change undo() works");

  editor.redo();
  is(editor.getText(), "foo3", "compound change redo() works");

  // Minimal keyboard usage tests.

  ok(editor.hasFocus(), "editor has focus");

  editor.setText("code-editor");
  editor.setCaretOffset(7);
  EventUtils.synthesizeKey(".", {}, testWin);

  is(editor.getText(), "code-ed.itor", "focus() and typing works");

  EventUtils.synthesizeKey("a", {}, testWin);

  is(editor.getText(), "code-ed.aitor", "typing works");

  is(editor.getCaretOffset(), 9, "caret moved");

  EventUtils.synthesizeKey("VK_LEFT", {}, testWin);

  is(editor.getCaretOffset(), 8, "caret moved to the left");

  EventUtils.synthesizeKey(".", {}, testWin);
  EventUtils.synthesizeKey("VK_TAB", {}, testWin);

  is(editor.getText(), "code-ed..     aitor", "Tab works");

  is(editor.getCaretOffset(), 14, "caret location is correct");

  // Test the Tab key.

  editor.setText("a\n  b\n c");
  editor.setCaretOffset(0);

  EventUtils.synthesizeKey("VK_TAB", {}, testWin);
  is(editor.getText(), "       a\n  b\n c", "Tab works");

  // Code editor specific tests. These are not applicable when the textarea
  // fallback is used.
  let component = Services.prefs.getCharPref(SourceEditor.PREFS.COMPONENT);
  if (component != "textarea") {
    editor.setMode(SourceEditor.MODES.JAVASCRIPT);
    is(editor.getMode(), SourceEditor.MODES.JAVASCRIPT, "setMode() works");

    editor.setSelection(0, editor.getCharCount() - 1);
    EventUtils.synthesizeKey("VK_TAB", {}, testWin);
    is(editor.getText(), "              a\n         b\n        c", "lines indented");

    EventUtils.synthesizeKey("VK_TAB", {shiftKey: true}, testWin);
    is(editor.getText(), "       a\n  b\n c", "lines outdented (shift-tab)");

    testEclipseBug362107();
    testBug687577();
    testBackspaceKey();
    testReturnKey();
  }

  // Test the read-only mode.

  editor.setText("foofoo");

  editor.readOnly = true;
  EventUtils.synthesizeKey("b", {}, testWin);
  is(editor.getText(), "foofoo", "editor is now read-only (keyboard)");

  editor.setText("foobar");
  is(editor.getText(), "foobar", "editor allows programmatic changes (setText)");

  EventUtils.synthesizeKey("VK_RETURN", {}, testWin);
  is(editor.getText(), "foobar", "Enter key does nothing");

  EventUtils.synthesizeKey("VK_TAB", {}, testWin);
  is(editor.getText(), "foobar", "Tab does nothing");

  editor.setText("      foobar");
  EventUtils.synthesizeKey("VK_TAB", {shiftKey: true}, testWin);
  is(editor.getText(), "      foobar", "Shift+Tab does nothing");

  editor.readOnly = false;

  editor.setCaretOffset(editor.getCharCount());
  EventUtils.synthesizeKey("-", {}, testWin);
  is(editor.getText(), "      foobar-", "editor is now editable again");

  // Test the Selection event.

  editor.setText("foobarbaz");

  editor.setSelection(1, 4);

  let event = null;

  let eventHandler = function(aEvent) {
    event = aEvent;
  };
  editor.addEventListener(SourceEditor.EVENTS.SELECTION, eventHandler);

  editor.setSelection(0, 3);

  ok(event, "selection event fired");
  ok(event.oldValue.start == 1 && event.oldValue.end == 4,
     "event.oldValue is correct");
  ok(event.newValue.start == 0 && event.newValue.end == 3,
     "event.newValue is correct");

  event = null;
  editor.dropSelection();

  ok(event, "selection dropped");
  ok(event.oldValue.start == 0 && event.oldValue.end == 3,
     "event.oldValue is correct");
  ok(event.newValue.start == 3 && event.newValue.end == 3,
     "event.newValue is correct");

  event = null;
  EventUtils.synthesizeKey("a", {accelKey: true}, testWin);

  ok(event, "select all worked");
  ok(event.oldValue.start == 3 && event.oldValue.end == 3,
     "event.oldValue is correct");
  ok(event.newValue.start == 0 && event.newValue.end == 9,
     "event.newValue is correct");

  event = null;
  editor.removeEventListener(SourceEditor.EVENTS.SELECTION, eventHandler);
  editor.dropSelection();

  ok(!event, "selection event listener removed");

  // Test the TextChanged event.

  editor.addEventListener(SourceEditor.EVENTS.TEXT_CHANGED, eventHandler);

  EventUtils.synthesizeKey(".", {}, testWin);

  ok(event, "the TextChanged event fired after keypress");
  is(event.start, 9, "event.start is correct");
  is(event.removedCharCount, 0, "event.removedCharCount is correct");
  is(event.addedCharCount, 1, "event.addedCharCount is correct");

  editor.setText("line1\nline2\nline3");
  let chars = editor.getText().length;
  event = null;

  editor.setText("a\nline4\nline5", chars);

  ok(event, "the TextChanged event fired after setText()");
  is(event.start, chars, "event.start is correct");
  is(event.removedCharCount, 0, "event.removedCharCount is correct");
  is(event.addedCharCount, 13, "event.addedCharCount is correct");

  event = null;
  editor.setText("line3b\nline4b\nfoo", 12, 24);

  ok(event, "the TextChanged event fired after setText() again");
  is(event.start, 12, "event.start is correct");
  is(event.removedCharCount, 12, "event.removedCharCount is correct");
  is(event.addedCharCount, 17, "event.addedCharCount is correct");

  editor.removeEventListener(SourceEditor.EVENTS.TEXT_CHANGED, eventHandler);

  testClipboardEvents();
}

function testEnd()
{
  editor.destroy();
  ok(!editor.parentElement && !editor.editorElement, "destroy() works");

  testWin.close();

  testWin = testDoc = editor = null;

  waitForFocus(finish, window);
}

function testBackspaceKey()
{
  editor.setText("       a\n  b\n c");
  editor.setCaretOffset(7);
  EventUtils.synthesizeKey("VK_BACK_SPACE", {}, testWin);
  is(editor.getText(), "a\n  b\n c", "line outdented (Backspace)");

  editor.undo();

  editor.setCaretOffset(6);
  EventUtils.synthesizeKey("VK_BACK_SPACE", {}, testWin);
  is(editor.getText(), "      a\n  b\n c", "backspace one char works");
}

function testReturnKey()
{
  editor.setText("       a\n  b\n c");

  editor.setCaretOffset(8);
  EventUtils.synthesizeKey("VK_RETURN", {}, testWin);
  EventUtils.synthesizeKey("x", {}, testWin);

  let lineDelimiter = editor.getLineDelimiter();
  ok(lineDelimiter, "we have the line delimiter");

  let indentationString = editor.getIndentationString();
  is("       ", indentationString, "we have an indentation string of 7 spaces");

  is(editor.getText(), "       a" + lineDelimiter + "       x\n  b\n c",
     "return maintains indentation");

  editor.setCaretOffset(12 + lineDelimiter.length);
  EventUtils.synthesizeKey("z", {}, testWin);
  EventUtils.synthesizeKey("VK_RETURN", {}, testWin);
  EventUtils.synthesizeKey("y", {}, testWin);
  is(editor.getText(), "       a" + lineDelimiter +
                       "    z" + lineDelimiter + "    yx\n  b\n c",
     "return maintains indentation (again)");
}

function testClipboardEvents()
{
  editor.setText("foobar");

  let doCut = function() {
    EventUtils.synthesizeKey("a", {accelKey: true}, testWin);

    is(editor.getSelectedText(), "foobar", "select all worked");

    EventUtils.synthesizeKey("x", {accelKey: true}, testWin);
  };

  let onCut = function() {
    ok(!editor.getText(), "cut works");
    editor.setText("test--");
    editor.setCaretOffset(5);

    editor.addEventListener(SourceEditor.EVENTS.TEXT_CHANGED, onPaste1);
    EventUtils.synthesizeKey("v", {accelKey: true}, testWin);
  };

  let onPaste1 = function() {
    editor.removeEventListener(SourceEditor.EVENTS.TEXT_CHANGED, onPaste1);

    is(editor.getText(), "test-foobar-", "paste works");

    executeSoon(waitForClipboard.bind(this, "test", doCopy, onCopy, testEnd));
  };

  let doCopy = function() {
    editor.setSelection(0, 4);
    EventUtils.synthesizeKey("c", {accelKey: true}, testWin);
  };

  let onCopy = function() {
    editor.setSelection(5, 11);
    editor.addEventListener(SourceEditor.EVENTS.TEXT_CHANGED, onPaste2);
    EventUtils.synthesizeKey("v", {accelKey: true}, testWin);
  };

  let pasteTextChanges = 0;
  let removedCharCount = 0;
  let addedCharCount = 0;
  let onPaste2 = function(aEvent) {
    pasteTextChanges++;
    ok(aEvent && (pasteTextChanges == 1 || pasteTextChanges == 2),
       "event TEXT_CHANGED fired " + pasteTextChanges + " time(s)");

    is(aEvent.start, 5, "event.start is correct");
    if (aEvent.removedCharCount) {
      removedCharCount = aEvent.removedCharCount;
    }
    if (aEvent.addedCharCount) {
      addedCharCount = aEvent.addedCharCount;
    }

    if (pasteTextChanges == 2 || addedCharCount && removedCharCount) {
      editor.removeEventListener(SourceEditor.EVENTS.TEXT_CHANGED, onPaste2);
      executeSoon(checkPaste2Result);
    }
  };

  let checkPaste2Result = function() {
    is(removedCharCount, 6, "event.removedCharCount is correct");
    is(addedCharCount, 4, "event.addedCharCount is correct");

    is(editor.getText(), "test-test-", "paste works after copy");
    testEnd();
  };

  waitForClipboard("foobar", doCut, onCut, testEnd);
}

function testEclipseBug362107()
{
  // Test for Eclipse Bug 362107:
  // https://bugs.eclipse.org/bugs/show_bug.cgi?id=362107
  let OS = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).OS;
  if (OS != "Linux") {
    return;
  }

  editor.setText("line 1\nline 2\nline 3");
  editor.setCaretOffset(16);

  EventUtils.synthesizeKey("VK_UP", {ctrlKey: true}, testWin);
  is(editor.getCaretOffset(), 9, "Ctrl-Up works");

  EventUtils.synthesizeKey("VK_UP", {ctrlKey: true}, testWin);
  is(editor.getCaretOffset(), 2, "Ctrl-Up works twice");

  EventUtils.synthesizeKey("VK_DOWN", {ctrlKey: true}, testWin);
  is(editor.getCaretOffset(), 9, "Ctrl-Down works");

  EventUtils.synthesizeKey("VK_DOWN", {ctrlKey: true}, testWin);
  is(editor.getCaretOffset(), 16, "Ctrl-Down works twice");
}

function testBug687577()
{
  // Test for Mozilla Bug 687577:
  // https://bugzilla.mozilla.org/show_bug.cgi?id=687577
  let OS = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).OS;
  if (OS != "Linux") {
    return;
  }

  editor.setText("line foobar 1\nline foobar 2\nline foobar 3");
  editor.setCaretOffset(39);

  EventUtils.synthesizeKey("VK_LEFT", {ctrlKey: true, shiftKey: true}, testWin);
  let selection = editor.getSelection();
  is(selection.start, 33, "select.start after Ctrl-Shift-Left");
  is(selection.end, 39, "select.end after Ctrl-Shift-Left");

  EventUtils.synthesizeKey("VK_UP", {ctrlKey: true, shiftKey: true}, testWin);
  selection = editor.getSelection();
  is(selection.start, 14, "select.start after Ctrl-Shift-Up");
  is(selection.end, 39, "select.end after Ctrl-Shift-Up");

  EventUtils.synthesizeKey("VK_UP", {ctrlKey: true, shiftKey: true}, testWin);
  selection = editor.getSelection();
  is(selection.start, 0, "select.start after Ctrl-Shift-Up (again)");
  is(selection.end, 39, "select.end after Ctrl-Shift-Up (again)");

  EventUtils.synthesizeKey("VK_DOWN", {ctrlKey: true, shiftKey: true}, testWin);
  selection = editor.getSelection();
  is(selection.start, 27, "select.start after Ctrl-Shift-Down");
  is(selection.end, 39, "select.end after Ctrl-Shift-Down");

  EventUtils.synthesizeKey("VK_DOWN", {ctrlKey: true, shiftKey: true}, testWin);
  selection = editor.getSelection();
  is(selection.start, 39, "select.start after Ctrl-Shift-Down (again)");
  is(selection.end, 41, "select.end after Ctrl-Shift-Down (again)");
}
