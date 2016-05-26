/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that pretty printing when the debugger is paused does not switch away
 * from the selected source.
 */

const TAB_URL = EXAMPLE_URL + "doc_pretty-print-on-paused.html";

var gTab, gPanel, gDebugger, gThreadClient, gSources;

const SECOND_SOURCE_VALUE = EXAMPLE_URL + "code_ugly-2.js";

function test() {
  // Wait for debugger panel to be fully set and break on debugger statement
  let options = {
    source: "code_script-switching-02.js",
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gThreadClient = gDebugger.gThreadClient;
    gSources = gDebugger.DebuggerView.Sources;

    Task.spawn(function* () {
      try {
        yield doInterrupt(gPanel);

        let source = gThreadClient.source(getSourceForm(gSources, SECOND_SOURCE_VALUE));
        yield source.setBreakpoint({
          line: 6
        });
        yield doResume(gPanel);

        const bpHit = waitForCaretAndScopes(gPanel, 6);
        callInTab(gTab, "secondCall");
        yield bpHit;

        info("Switch to the second source.");
        const sourceShown = waitForSourceShown(gPanel, SECOND_SOURCE_VALUE);
        gSources.selectedValue = getSourceActor(gSources, SECOND_SOURCE_VALUE);
        yield sourceShown;

        info("Pretty print the source.");
        const prettyPrinted = waitForSourceShown(gPanel, SECOND_SOURCE_VALUE);
        gDebugger.document.getElementById("pretty-print").click();
        yield prettyPrinted;

        yield resumeDebuggerThenCloseAndFinish(gPanel);
      } catch (e) {
        DevToolsUtils.reportException("browser_dbg_pretty-print-on-paused.js", e);
        ok(false, "Got an error: " + DevToolsUtils.safeErrorString(e));
      }
    });
  });
}

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gThreadClient = null;
  gSources = null;
});
