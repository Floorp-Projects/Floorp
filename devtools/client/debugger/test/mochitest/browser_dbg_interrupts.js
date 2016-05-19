/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test resuming from button and keyboard shortcuts.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

function test() {
  let gTab, gPanel, gDebugger;
  let gSources, gBreakpoints, gTarget, gResumeButton, gResumeKey, gThreadClient;

  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
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
    ok(false, "A pause was sent, but it shouldn't have been");
  }

  function addBreakpoints() {
    return promise.resolve(null)
      .then(() => gPanel.addBreakpoint({ actor: gSources.values[0], line: 5 }))
      .then(() => gPanel.addBreakpoint({ actor: gSources.values[1], line: 6 }))
      .then(() => gPanel.addBreakpoint({ actor: gSources.values[1], line: 7 }))
      .then(() => ensureThreadClientState(gPanel, "resumed"));
  }

  function resume() {
    let onceResumed = gTarget.once("thread-resumed");
    gThreadClient.resume();
    return onceResumed;
  }

  function testResumeButton() {
    info("Pressing the resume button, expecting a thread-paused");

    ok(!gResumeButton.hasAttribute("disabled"), "Resume button is not disabled 1");
    ok(!gResumeButton.hasAttribute("break-on-next"), "Resume button isn't waiting for next execution");
    ok(!gResumeButton.hasAttribute("checked"), "Resume button is not checked");
    let oncePaused = gTarget.once("thread-paused");

    // Click the pause button to break on next execution
    EventUtils.sendMouseEvent({ type: "mousedown" }, gResumeButton, gDebugger);
    ok(gResumeButton.hasAttribute("disabled"), "Resume button is disabled");
    ok(gResumeButton.hasAttribute("break-on-next"), "Resume button is waiting for next execution");
    ok(!gResumeButton.hasAttribute("checked"), "Resume button is not checked");

    // Evaluate a script to fully pause the debugger
    once(gDebugger.gClient, "willInterrupt").then(() => {
      evalInTab(gTab, "1+1;");
    });

    return waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN)
      .then(() => {
        ok(!gResumeButton.hasAttribute("break-on-next"), "Resume button isn't waiting for next execution");
        is(gResumeButton.getAttribute("checked"), "true", "Resume button is checked");
        ok(!gResumeButton.hasAttribute("disabled"), "Resume button is not disabled 2");
      })
      .then(() => {
        let p = ensureThreadClientState(gPanel, "resumed");
        gThreadClient.resume();
        return p;
      });
  }

  function testResumeKeyboard() {
    let key = gResumeKey.getAttribute("keycode");
    info("Triggering a pause with keyboard (" + key + "), expecting a thread-paused");

    ok(!gResumeButton.hasAttribute("disabled"), "Resume button is not disabled 3");
    ok(!gResumeButton.hasAttribute("break-on-next"), "Resume button isn't waiting for next execution");
    ok(!gResumeButton.hasAttribute("checked"), "Resume button is not checked");

    // Press the key to break on next execution
    EventUtils.synthesizeKey(key, { }, gDebugger);
    ok(gResumeButton.hasAttribute("disabled"), "Resume button is disabled");
    ok(gResumeButton.hasAttribute("break-on-next"), "Resume button is waiting for next execution");
    ok(!gResumeButton.hasAttribute("checked"), "Resume button is not checked");

    // Evaluate a script to fully pause the debugger
    once(gDebugger.gClient, "willInterrupt").then(() => {
      evalInTab(gTab, "1+1;");
    });

    return waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN)
      .then(() => {
        ok(!gResumeButton.hasAttribute("break-on-next"), "Resume button isn't waiting for next execution");
        is(gResumeButton.getAttribute("checked"), "true", "Resume button is checked");
        ok(!gResumeButton.hasAttribute("disabled"), "Resume button is not disabled 4");
      })
      .then(() => {
        let p = ensureThreadClientState(gPanel, "resumed");
        gThreadClient.resume();
        return p;
      });
  }
}
