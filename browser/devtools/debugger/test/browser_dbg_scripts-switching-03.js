/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the DebuggerView error loading source text is correct.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

let gTab, gPanel, gDebugger;
let gView, gEditor, gL10N;

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gView = gDebugger.DebuggerView;
    gEditor = gDebugger.DebuggerView.editor;
    gL10N = gDebugger.L10N;

    waitForSourceShown(gPanel, "-01.js")
      .then(showBogusSource)
      .then(testDebuggerLoadingError)
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function showBogusSource() {
  let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_ERROR_SHOWN);
  gView._setEditorSource({ url: "http://example.com/fake.js", actor: "fake.actor" });
  return finished;
}

function testDebuggerLoadingError() {
  ok(gEditor.getText().includes(gL10N.getStr("errorLoadingText")),
    "The valid error loading message is displayed.");
}

registerCleanupFunction(function() {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gView = null;
  gEditor = null;
  gL10N = null;
});
