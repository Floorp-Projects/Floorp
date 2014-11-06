/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that we have the correct line selected after pretty printing.
 */

const TAB_URL = EXAMPLE_URL + "doc_pretty-print.html";

let gTab, gPanel, gDebugger;

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;

    waitForSourceShown(gPanel, "code_ugly.js")
      .then(runCodeAndPause)
      .then(() => {
        const sourceShown = waitForSourceShown(gPanel, "code_ugly.js");
        const caretUpdated = waitForCaretUpdated(gPanel, 7);
        const finished = promise.all([sourceShown, caretUpdated]);
        clickPrettyPrintButton();
        return finished;
      })
      .then(resumeDebuggerThenCloseAndFinish.bind(null, gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + DevToolsUtils.safeErrorString(aError));
      });
  });
}

function runCodeAndPause() {
  const deferred = promise.defer();
  once(gDebugger.gThreadClient, "paused").then(deferred.resolve);
  callInTab(gTab, "foo");
  return deferred.promise;
}

function clickPrettyPrintButton() {
  gDebugger.document.getElementById("pretty-print").click();
}

registerCleanupFunction(function() {
  gTab = null;
  gPanel = null;
  gDebugger = null;
});
