/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the selection is dropped for line and token searches, after
 * pressing backspace enough times.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

var gTab, gPanel, gDebugger;
var gEditor, gSources, gSearchBox;

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gSearchBox = gDebugger.DebuggerView.Filtering._searchbox;

    waitForSourceShown(gPanel, "-01.js")
      .then(testLineSearch)
      .then(testTokenSearch)
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function testLineSearch() {
  setText(gSearchBox, ":42");
  ok(isCaretPos(gPanel, 7),
    "The editor caret position appears to be correct (1.1).");
  ok(isEditorSel(gPanel, [151, 151]),
    "The editor selection appears to be correct (1.1).");
  is(gEditor.getSelection(), "",
    "The editor selected text appears to be correct (1.1).");

  backspaceText(gSearchBox, 1);
  ok(isCaretPos(gPanel, 4),
    "The editor caret position appears to be correct (1.2).");
  ok(isEditorSel(gPanel, [110, 110]),
    "The editor selection appears to be correct (1.2).");
  is(gEditor.getSelection(), "",
    "The editor selected text appears to be correct (1.2).");

  backspaceText(gSearchBox, 1);
  ok(isCaretPos(gPanel, 4),
    "The editor caret position appears to be correct (1.3).");
  ok(isEditorSel(gPanel, [110, 110]),
    "The editor selection appears to be correct (1.3).");
  is(gEditor.getSelection(), "",
    "The editor selected text appears to be correct (1.3).");

  setText(gSearchBox, ":4");
  ok(isCaretPos(gPanel, 4),
    "The editor caret position appears to be correct (1.4).");
  ok(isEditorSel(gPanel, [110, 110]),
    "The editor selection appears to be correct (1.4).");
  is(gEditor.getSelection(), "",
    "The editor selected text appears to be correct (1.4).");

  gSearchBox.select();
  backspaceText(gSearchBox, 1);
  ok(isCaretPos(gPanel, 4),
    "The editor caret position appears to be correct (1.5).");
  ok(isEditorSel(gPanel, [110, 110]),
    "The editor selection appears to be correct (1.5).");
  is(gEditor.getSelection(), "",
    "The editor selected text appears to be correct (1.5).");
  is(gSearchBox.value, "",
    "The searchbox should have been cleared.");
}

function testTokenSearch() {
  setText(gSearchBox, "#();");
  ok(isCaretPos(gPanel, 5, 16),
    "The editor caret position appears to be correct (2.1).");
  ok(isEditorSel(gPanel, [145, 148]),
    "The editor selection appears to be correct (2.1).");
  is(gEditor.getSelection(), "();",
    "The editor selected text appears to be correct (2.1).");

  backspaceText(gSearchBox, 1);
  ok(isCaretPos(gPanel, 4, 21),
    "The editor caret position appears to be correct (2.2).");
  ok(isEditorSel(gPanel, [128, 130]),
    "The editor selection appears to be correct (2.2).");
  is(gEditor.getSelection(), "()",
    "The editor selected text appears to be correct (2.2).");

  backspaceText(gSearchBox, 2);
  ok(isCaretPos(gPanel, 4, 20),
    "The editor caret position appears to be correct (2.3).");
  ok(isEditorSel(gPanel, [129, 129]),
    "The editor selection appears to be correct (2.3).");
  is(gEditor.getSelection(), "",
    "The editor selected text appears to be correct (2.3).");

  setText(gSearchBox, "#;");
  ok(isCaretPos(gPanel, 5, 16),
    "The editor caret position appears to be correct (2.4).");
  ok(isEditorSel(gPanel, [147, 148]),
    "The editor selection appears to be correct (2.4).");
  is(gEditor.getSelection(), ";",
    "The editor selected text appears to be correct (2.4).");

  gSearchBox.select();
  backspaceText(gSearchBox, 1);
  ok(isCaretPos(gPanel, 5, 16),
    "The editor caret position appears to be correct (2.5).");
  ok(isEditorSel(gPanel, [148, 148]),
    "The editor selection appears to be correct (2.5).");
  is(gEditor.getSelection(), "",
    "The editor selected text appears to be correct (2.5).");
  is(gSearchBox.value, "",
    "The searchbox should have been cleared.");
}

registerCleanupFunction(function() {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gEditor = null;
  gSources = null;
  gSearchBox = null;
});
