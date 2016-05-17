/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the message which breakpoint condition throws
 * could be displayed on UI correctly
 */

const TAB_URL = EXAMPLE_URL + "doc_conditional-breakpoints.html";

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gEditor = gDebugger.DebuggerView.editor;
    const gSources = gDebugger.DebuggerView.Sources;
    const actions = bindActionCreators(gPanel);
    const getState = gDebugger.DebuggerController.getState;

    function initialCheck(aCaretLine) {
      let bp = gDebugger.queries.getBreakpoint(getState(),
                                               { actor: gSources.values[0], line: aCaretLine });
      ok(bp, "There should be a breakpoint on line " + aCaretLine);

      let attachment = gSources._getBreakpoint(bp).attachment;
      ok(attachment,
         "There should be a breakpoint on line " + aCaretLine + " in the sources pane.");

      let thrownNode = attachment.view.container.querySelector(".dbg-breakpoint-condition-thrown-message");
      ok(thrownNode,
         "The breakpoint item should contain a thrown message node.");

      ok(!attachment.view.container.classList.contains("dbg-breakpoint-condition-thrown"),
         "The thrown message on line " + aCaretLine + " should be hidden when condition has not been evaluated.");
    }

    function resumeAndTestThrownMessage(line) {
      doResume(gPanel);

      return waitForCaretUpdated(gPanel, line).then(() => {
        // Test that the thrown message is correctly shown.
        let bp = gDebugger.queries.getBreakpoint(
          getState(),
          { actor: gSources.values[0], line: line }
        );
        let attachment = gSources._getBreakpoint(bp).attachment;
        ok(attachment.view.container.classList.contains("dbg-breakpoint-condition-thrown"),
           "Message on line " + line + " should be shown when condition throws.");
      });
    }

    function resumeAndTestNoThrownMessage(line) {
      doResume(gPanel);

      return waitForCaretUpdated(gPanel, line).then(() => {
        // test that the thrown message is correctly shown
        let bp = gDebugger.queries.getBreakpoint(
          getState(),
          { actor: gSources.values[0], line: line }
        );
        let attachment = gSources._getBreakpoint(bp).attachment;
        ok(!attachment.view.container.classList.contains("dbg-breakpoint-condition-thrown"),
           "Message on line " + line + " should be hidden if condition doesn't throw.");
      });
    }

    Task.spawn(function* () {
      yield waitForSourceAndCaretAndScopes(gPanel, ".html", 17);

      yield actions.addBreakpoint({ actor: gSources.selectedValue, line: 18 }, " 1afff");
      // Close the popup because a SET_BREAKPOINT_CONDITION action is
      // fired when it's closed, and it sets it on the currently
      // selected breakpoint and we want to make sure it uses the
      // current breakpoint. This isn't a problem outside of tests
      // because any UI interaction will close the popup before the
      // new breakpoint is added.
      gSources._hideConditionalPopup();
      initialCheck(18);

      yield actions.addBreakpoint({ actor: gSources.selectedValue, line: 19 }, "true");
      gSources._hideConditionalPopup();
      initialCheck(19);

      yield actions.addBreakpoint({ actor: gSources.selectedValue, line: 20 }, "false");
      gSources._hideConditionalPopup();
      initialCheck(20);

      yield actions.addBreakpoint({ actor: gSources.selectedValue, line: 22 }, "randomVar");
      gSources._hideConditionalPopup();
      initialCheck(22);

      yield resumeAndTestThrownMessage(18);
      yield resumeAndTestNoThrownMessage(19);
      yield resumeAndTestThrownMessage(22);
      resumeDebuggerThenCloseAndFinish(gPanel);
    });

    callInTab(gTab, "ermahgerd");
  });
}
