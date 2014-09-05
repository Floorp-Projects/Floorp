/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Evaluating two functions on the same line and checking for correct hit count
 * for both of them in CodeMirror's gutter.
 */

const TAB_URL = EXAMPLE_URL + "doc_same-line-functions.html";
const CODE_URL = "code_same-line-functions.js";

let gTab, gDebuggee, gPanel, gDebugger;
let gEditor;

function test() {
  Task.async(function* () {
    yield pushPrefs(["devtools.debugger.tracer", true]);

    initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
      gTab = aTab;
      gDebuggee = aDebuggee;
      gPanel = aPanel;
      gDebugger = gPanel.panelWin;
      gEditor = gDebugger.DebuggerView.editor;

      Task.async(function* () {
        yield waitForSourceShown(gPanel, CODE_URL);
        yield startTracing(gPanel);

        clickButton();

        yield waitForClientEvents(aPanel, "traces");

        testHitCounts();

        yield stopTracing(gPanel);
        yield popPrefs();
        yield closeDebuggerAndFinish(gPanel);
      })();
    });
  })().catch(e => {
    ok(false, "Got an error: " + e.message + "\n" + e.stack);
  });
}

function clickButton() {
  EventUtils.sendMouseEvent({ type: "click" },
                            gDebuggee.document.querySelector("button"),
                            gDebuggee);
}

function testHitCounts() {
  let marker = gEditor.getMarker(0, 'hit-counts');

  is(marker.innerHTML, "1\u00D7|1\u00D7",
    "Both functions should be hit only once.");
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gEditor = null;
});