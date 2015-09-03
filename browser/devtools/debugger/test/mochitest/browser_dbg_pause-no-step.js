/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that pausing / stepping is only enabled when there is a
 * location.
 */

const TAB_URL = EXAMPLE_URL + "doc_pause-exceptions.html";

let gTab, gPanel, gDebugger;
let gResumeButton, gStepOverButton, gStepOutButton, gStepInButton;
let gResumeKey, gFrames;

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
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
    ok(gDebugger.gThreadClient.paused,
       "Should be paused after an interrupt request.");

    ok(!gStepOutButton.disabled, "Stepping out button should be enabled");
    ok(!gStepInButton.disabled, "Stepping in button should be enabled");
    ok(!gStepOverButton.disabled, "Stepping over button should be enabled");

    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.AFTER_FRAMES_REFILLED).then(() => {
      is(gFrames.itemCount, 1,
         "Should have 1 frame from the evalInTab call.");
      gDebugger.gThreadClient.resume(testBreakAtLocation);
    });

  });

  EventUtils.sendMouseEvent({ type: "mousedown" }, gResumeButton, gDebugger);

  ok(!gDebugger.gThreadClient.paused,
    "Shouldn't be paused until the next script is executed.");
  ok(gStepOutButton.disabled, "Stepping out button should be disabled");
  ok(gStepInButton.disabled, "Stepping in button should be disabled");
  ok(gStepOverButton.disabled, "Stepping over button should be disabled");

  // Evaluate a script to fully pause the debugger
  once(gDebugger.gClient, "willInterrupt").then(() => {
    evalInTab(gTab, "1+1;");
  });
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
