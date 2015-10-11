/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the jump to function definition works properly.
 */

const TAB_URL = EXAMPLE_URL + "doc_function-jump.html";
const SCRIPT_URI = EXAMPLE_URL + "code_function-jump-01.js";


function test() {
  let gTab, gPanel, gDebugger, gSources;

  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gSources = gDebugger.DebuggerView.Sources;

    waitForSourceShown(gPanel, "-01.js")
      .then(jumpToFunctionDefinition)
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });

  function jumpToFunctionDefinition() {
    let callLocation = {line: 5, ch: 0};
    let editor = gDebugger.DebuggerView.editor;
    let coords = editor.getCoordsFromPosition(callLocation);

    gDebugger.DebuggerView.Sources._onMouseDown({ clientX: coords.left,
                                                  clientY: coords.top,
                                                  metaKey: true });

    let deferred = promise.defer();
    executeSoon(() => {
      is(editor.getDebugLocation(), 1, "foo definition should be highlighted");
      deferred.resolve();
    });
    return deferred.promise;
  }
}
