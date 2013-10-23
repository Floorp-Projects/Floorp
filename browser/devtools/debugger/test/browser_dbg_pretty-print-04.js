/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the function searching works with pretty printed sources.
 */

const TAB_URL = EXAMPLE_URL + "doc_pretty-print.html";

let gTab, gDebuggee, gPanel, gDebugger;
let gSearchBox;

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gSearchBox = gDebugger.DebuggerView.Filtering._searchbox;

    waitForSourceShown(gPanel, "code_ugly.js")
      .then(testUglySearch)
      .then(() => {
        const finished = waitForSourceShown(gPanel, "code_ugly.js");
        clickPrettyPrintButton();
        return finished;
      })
      .then(testPrettyPrintedSearch)
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function testUglySearch() {
  const deferred = promise.defer();

  once(gDebugger, "popupshown").then(() => {
    ok(isCaretPos(gPanel, 2, 10),
       "The bar function's non-pretty-printed location should be shown.");
    deferred.resolve();
  });

  setText(gSearchBox, "@bar");
  return deferred.promise;
}

function clickPrettyPrintButton() {
  gDebugger.document.getElementById("pretty-print").click();
}

function testPrettyPrintedSearch() {
  const deferred = promise.defer();

  once(gDebugger, "popupshown").then(() => {
    ok(isCaretPos(gPanel, 6, 10),
       "The bar function's pretty printed location should be shown.");
    deferred.resolve();
  });

  setText(gSearchBox, "@bar");
  return deferred.promise;
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gSearchBox = null;
});
