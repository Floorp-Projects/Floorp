/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the searchbox popup isn't displayed when there's some text
 * already present.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

var gTab, gPanel, gDebugger;
var gEditor, gSearchBox, gSearchBoxPanel;

function test() {
  let options = {
    source: EXAMPLE_URL + "code_script-switching-01.js",
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSearchBox = gDebugger.DebuggerView.Filtering._searchbox;
    gSearchBoxPanel = gDebugger.DebuggerView.Filtering._searchboxHelpPanel;

    once(gSearchBoxPanel, "popupshown").then(() => {
      ok(false, "Damn it, this shouldn't have happened.");
    });

    waitForSourceAndCaretAndScopes(gPanel, "-02.js", 1)
      .then(tryShowPopup)
      .then(focusEditor)
      .then(testFocusLost)
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    callInTab(gTab, "firstCall");
  });
}

function tryShowPopup() {
  setText(gSearchBox, "#call()");
  ok(isCaretPos(gPanel, 4, 22),
    "The editor caret position appears to be correct.");
  ok(isEditorSel(gPanel, [125, 131]),
    "The editor selection appears to be correct.");
  is(gEditor.getSelection(), "Call()",
    "The editor selected text appears to be correct.");

  is(gSearchBoxPanel.state, "closed",
    "The search box panel shouldn't be visible yet.");

  EventUtils.sendMouseEvent({ type: "click" }, gSearchBox, gDebugger);
}

function focusEditor() {
  let deferred = promise.defer();

  // Focusing the editor takes a tick to update the caret and selection.
  gEditor.focus();
  executeSoon(deferred.resolve);

  return deferred.promise;
}

function testFocusLost() {
  ok(isCaretPos(gPanel, 4, 22),
    "The editor caret position appears to be correct after gaining focus.");
  ok(isEditorSel(gPanel, [125, 131]),
    "The editor selection appears to be correct after gaining focus.");
  is(gEditor.getSelection(), "Call()",
    "The editor selected text appears to be correct after gaining focus.");

  is(gSearchBoxPanel.state, "closed",
    "The search box panel should still not be visible.");
}

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gEditor = null;
  gSearchBox = null;
  gSearchBoxPanel = null;
});
