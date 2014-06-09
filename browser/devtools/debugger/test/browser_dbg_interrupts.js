/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test if the breakpoints toggle button works as advertised.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

function test() {
  let gTab, gDebuggee, gPanel, gDebugger;
  let gSources, gBreakpoints, gTarget, gResumeButton, gResumeKey, gThreadClient;

  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gSources = gDebugger.DebuggerView.Sources;
    gBreakpoints = gDebugger.DebuggerController.Breakpoints;
    gTarget = gDebugger.gTarget;
    gThreadClient = gDebugger.gThreadClient;
    gResumeButton = gDebugger.document.getElementById("resume");
    gResumeKey = gDebugger.document.getElementById("resumeKey");

    waitForSourceShown(gPanel, "-01.js")
      .then(() => { gTarget.on("thread-paused", failOnPause); })
      .then(addBreakpoints)
      .then(() => { gTarget.off("thread-paused", failOnPause); })
      .then(testResumeButton)
      .then(testResumeKeyboard)
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });

  function failOnPause() {
    ok (false, "A pause was sent, but it shouldn't have been");
  }

  function addBreakpoints() {
    return promise.resolve(null)
      .then(() => gPanel.addBreakpoint({ url: gSources.values[0], line: 5 }))
      .then(() => gPanel.addBreakpoint({ url: gSources.values[1], line: 6 }))
      .then(() => gPanel.addBreakpoint({ url: gSources.values[1], line: 7 }))
      .then(() => ensureThreadClientState(gPanel, "resumed"));
  }

  function resume() {
    let onceResumed = gTarget.once("thread-resumed");
    gThreadClient.resume();
    return onceResumed;
  }

  function testResumeButton() {
    info ("Pressing the resume button, expecting a thread-paused");

    ok (!gResumeButton.hasAttribute("checked"), "Resume button is not checked");
    let oncePaused = gTarget.once("thread-paused");
    EventUtils.sendMouseEvent({ type: "mousedown" }, gResumeButton, gDebugger);

    return oncePaused
      .then(() => {
        is (gResumeButton.getAttribute("checked"), "true", "Resume button is checked");
      })
      .then(() => gThreadClient.resume())
      .then(() => ensureThreadClientState(gPanel, "resumed"))
  }

  function testResumeKeyboard() {
    let key = gResumeKey.getAttribute("keycode");
    info ("Triggering a pause with keyboard (" + key +  "), expecting a thread-paused");

    ok (!gResumeButton.hasAttribute("checked"), "Resume button is not checked");
    let oncePaused = gTarget.once("thread-paused");
    EventUtils.synthesizeKey(key, { }, gDebugger);

    return oncePaused
      .then(() => {
        is (gResumeButton.getAttribute("checked"), "true", "Resume button is checked");
      })
      .then(() => gThreadClient.resume())
      .then(() => ensureThreadClientState(gPanel, "resumed"))
  }
}
