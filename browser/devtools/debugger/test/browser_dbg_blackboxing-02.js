/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that black boxed frames are compressed into a single frame on the stack
 * view.
 */

const TAB_URL = EXAMPLE_URL + "doc_blackboxing.html";
const BLACKBOXME_URL = EXAMPLE_URL + "code_blackboxing_blackboxme.js"

let gTab, gDebuggee, gPanel, gDebugger;
let gFrames;

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gFrames = gDebugger.DebuggerView.StackFrames;

    waitForSourceShown(gPanel, BLACKBOXME_URL)
      .then(testBlackBoxSource)
      .then(testBlackBoxStack)
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function testBlackBoxSource() {
  let finished = waitForThreadEvents(gPanel, "blackboxchange").then(aSource => {
    ok(aSource.isBlackBoxed, "The source should be black boxed now.");
  });

  getBlackBoxCheckbox(BLACKBOXME_URL).click();
  return finished;
}

function testBlackBoxStack() {
  let finished = waitForSourceAndCaretAndScopes(gPanel, ".html", 21).then(() => {
    is(gFrames.itemCount, 3,
      "Should only get 3 frames.");
    is(gDebugger.document.querySelectorAll(".dbg-stackframe-black-boxed").length, 1,
      "And one of them should be the combined black boxed frames.");
  });

  // Spin the event loop before causing the debuggee to pause, to allow
  // this function to return first.
  executeSoon(() => gDebuggee.runTest());
  return finished;
}

function getBlackBoxCheckbox(aUrl) {
  return gDebugger.document.querySelector(
    ".side-menu-widget-item[tooltiptext=\"" + aUrl + "\"] " +
    ".side-menu-widget-item-checkbox");
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gFrames = null;
});
