/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the message which breakpoint condition throws
 * could be displayed on UI correctly
 */

const TAB_URL = EXAMPLE_URL + "doc_conditional-breakpoints.html";

function test() {
  let gTab, gPanel, gDebugger, gEditor;
  let gSources, gLocation;

  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;

    waitForSourceAndCaretAndScopes(gPanel, ".html", 17)
      .then(addBreakpoints)
      .then(() => resumeAndTestThrownMessage(18))
      .then(() => resumeAndTestNoThrownMessage(19))
      .then(() => resumeAndTestThrownMessage(22))
      .then(() => resumeAndFinishTest())
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    callInTab(gTab, "ermahgerd");
  });

  function resumeAndTestThrownMessage(aLine) {
    EventUtils.sendMouseEvent({ type: "mousedown" },
      gDebugger.document.getElementById("resume"),
      gDebugger);

    let finished = waitForCaretUpdated(gPanel, aLine).then(() => {
      //test that the thrown message is correctly shown
      let attachment = gSources.getBreakpoint({ actor: gSources.values[0], line: aLine}).attachment;
      ok(attachment.view.container.classList.contains('dbg-breakpoint-condition-thrown'),
        "Message on line " + aLine + " should be shown when condition throws.");
    });

    return finished;
  }

  function resumeAndTestNoThrownMessage(aLine) {
    EventUtils.sendMouseEvent({ type: "mousedown" },
      gDebugger.document.getElementById("resume"),
      gDebugger);

    let finished = waitForCaretUpdated(gPanel, aLine).then(() => {
      //test that the thrown message is correctly shown
      let attachment = gSources.getBreakpoint({ actor: gSources.values[0], line: aLine}).attachment;
      ok(!attachment.view.container.classList.contains("dbg-breakpoint-condition-thrown"),
        "Message on line " + aLine + " should be hidden if condition doesn't throw.");
    });
    return finished;
  }

  function resumeAndFinishTest() {
    let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.AFTER_FRAMES_CLEARED)

    gDebugger.gThreadClient.resume();

    return finished;
  }

  function addBreakpoints() {
    return promise.resolve(null)
      .then(() => gPanel.addBreakpoint({ actor: gSources.selectedValue,
                                         line: 18,
                                         condition: " 1a"}))
      .then(() => initialCheck(18))
      .then(() => gPanel.addBreakpoint({ actor: gSources.selectedValue,
                                         line: 19,
                                         condition: "true"}))
      .then(() => initialCheck(19))
      .then(() => gPanel.addBreakpoint({ actor: gSources.selectedValue,
                                         line: 20,
                                         condition: "false"}))
      .then(() => initialCheck(20))
      .then(() => gPanel.addBreakpoint({ actor: gSources.selectedValue,
                                         line: 22,
                                         condition: "randomVar"}))
      .then(() => initialCheck(22));
  }

  function initialCheck(aCaretLine) {
    let bp = gSources.getBreakpoint({ actor: gSources.values[0], line: aCaretLine})
    let attachment = bp.attachment;
    ok(attachment,
      "There should be an item for line " + aCaretLine + " in the sources pane.");

    let thrownNode = attachment.view.container.querySelector(".dbg-breakpoint-condition-thrown-message");
    ok(thrownNode,
      "The breakpoint item should contain a thrown message node.")

    ok(!attachment.view.container.classList.contains("dbg-breakpoint-condition-thrown"),
      "The thrown message on line " + aCaretLine + " should be hidden when condition has not been evaluated.")
  }
}
