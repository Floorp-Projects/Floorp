/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource:///modules/source-editor.jsm");

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
    placeholderText: "foobarbaz",
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

  is(editor.getMode(), SourceEditor.DEFAULTS.MODE, "default editor mode");

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

  EventUtils.synthesizeKey("a", {accelKey: true}, testWin);

  is(editor.getSelectedText(), "code-ed.aitor",
     "select all worked");

  EventUtils.synthesizeKey("x", {accelKey: true}, testWin);

  ok(!editor.getText(), "cut works");

  EventUtils.synthesizeKey("v", {accelKey: true}, testWin);
  EventUtils.synthesizeKey("v", {accelKey: true}, testWin);

  is(editor.getText(), "code-ed.aitorcode-ed.aitor", "paste works");

  editor.setText("foo");

  EventUtils.synthesizeKey("a", {accelKey: true}, testWin);
  EventUtils.synthesizeKey("c", {accelKey: true}, testWin);
  EventUtils.synthesizeKey("v", {accelKey: true}, testWin);
  EventUtils.synthesizeKey("v", {accelKey: true}, testWin);

  is(editor.getText(), "foofoo", "ctrl-a, c, v, v works");

  is(editor.getCaretOffset(), 6, "caret location is correct");

  EventUtils.synthesizeKey(".", {}, testWin);
  EventUtils.synthesizeKey("VK_TAB", {}, testWin);

  is(editor.getText(), "foofoo.       ", "Tab works");

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

  editor.readOnly = false;

  editor.setCaretOffset(editor.getCharCount());
  EventUtils.synthesizeKey("-", {}, testWin);
  is(editor.getText(), "foobar-", "editor is now editable again");

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

  let chars = editor.getText().length;
  event = null;

  EventUtils.synthesizeKey("a", {accelKey: true}, testWin);
  EventUtils.synthesizeKey("c", {accelKey: true}, testWin);

  editor.setCaretOffset(chars);

  EventUtils.synthesizeKey("v", {accelKey: true}, testWin);

  ok(event, "the TextChanged event fired after paste");
  is(event.start, chars, "event.start is correct");
  is(event.removedCharCount, 0, "event.removedCharCount is correct");
  is(event.addedCharCount, chars, "event.addedCharCount is correct");

  editor.setText("line1\nline2\nline3");
  chars = editor.getText().length;

  event = null;

  editor.setText("a\nline4\nline5", chars);

  ok(event, "the TextChanged event fired after setText()");
  is(event.start, chars, "event.start is correct");
  is(event.removedCharCount, 0, "event.removedCharCount is correct");
  is(event.addedCharCount, 13, "event.addedCharCount is correct");

  editor.setText("line3b\nline4b\nfoo", 12, 24);

  ok(event, "the TextChanged event fired after setText() again");
  is(event.start, 12, "event.start is correct");
  is(event.removedCharCount, 12, "event.removedCharCount is correct");
  is(event.addedCharCount, 17, "event.addedCharCount is correct");

  editor.removeEventListener(SourceEditor.EVENTS.TEXT_CHANGED, eventHandler);

  // Done.

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

