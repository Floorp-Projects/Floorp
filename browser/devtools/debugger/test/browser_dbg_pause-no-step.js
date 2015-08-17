/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if stepping only is enabled when paused with a location
 */

const TAB_URL = EXAMPLE_URL + "doc_pause-exceptions.html";

let gPanel, gDebugger;
let gResumeButton, gStepOverButton, gStepOutButton, gStepInButton;
let gResumeKey, gFrames;

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gResumeButton = gDebugger.document.getElementById("resume");
    gStepOverButton = gDebugger.document.getElementById("step-over");
    gStepInButton = gDebugger.document.getElementById("step-in");
    gStepOutButton = gDebugger.document.getElementById("step-out");
    gResumeKey = gDebugger.document.getElementById("resumeKey");
    gFrames = gDebugger.DebuggerView.StackFrames;

    testPause();
  });
}

function testPause() {
  ok(!gDebugger.gThreadClient.paused, "Should be running after starting the test.");
  ok(gStepOutButton.disabled, "Stepping out button should be disabled");
  ok(gStepInButton.disabled, "Stepping in button should be disabled");
  ok(gStepOverButton.disabled, "Stepping over button should be disabled");

  gDebugger.gThreadClient.addOneTimeListener("paused", () => {
    // Nothing should happen here because the button is disabled. If
    // this runs any code, there will be errors and the test will fail.
    EventUtils.sendMouseEvent({ type: "mousedown" }, gStepOverButton, gDebugger);

    ok(gDebugger.gThreadClient.paused,
       "Should be paused after an interrupt request.");

    ok(gStepOutButton.disabled, "Stepping out button should still be disabled");
    ok(gStepInButton.disabled, "Stepping in button should still be disabled");
    ok(gStepOverButton.disabled, "Stepping over button should still be disabled");

    is(gFrames.itemCount, 0,
       "Should have no frames when paused in the main loop.");

    gDebugger.gThreadClient.resume(testBreakAtLocation);
  });

  EventUtils.sendMouseEvent({ type: "mousedown" }, gResumeButton, gDebugger);
}

function testBreakAtLocation() {
  gDebugger.gThreadClient.addOneTimeListener("paused", () => {
    ok(!gStepOutButton.disabled, "Stepping out button should be enabled");
    ok(!gStepInButton.disabled, "Stepping in button should be enabled");
    ok(!gStepOverButton.disabled, "Stepping over button should be enabled");

    resumeDebuggerThenCloseAndFinish(gPanel);
  });

  BrowserTestUtils.synthesizeMouseAtCenter('button', {}, gBrowser.selectedBrowser);
}

registerCleanupFunction(function() {
  gPanel = null;
  gDebugger = null;
  gResumeButton = null;
  gStepOverButton = null;
  gStepInButton = null;
  gStepOutButton = null;
  gResumeKey = null;
  gFrames = null;
});
