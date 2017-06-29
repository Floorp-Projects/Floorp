/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that debuggee scripts are terminated on tab closure.
 */

// The following rejection should not be left uncaught. This test has been
// whitelisted until the issue is fixed.
if (!gMultiProcessBrowser) {
  Cu.import("resource://testing-common/PromiseTestUtils.jsm", this);
  PromiseTestUtils.expectUncaughtRejection(/error\.message is undefined/);
}

const TAB_URL = EXAMPLE_URL + "doc_terminate-on-tab-close.html";

function test() {
  let options = {
    source: TAB_URL,
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;

    gDebugger.gThreadClient.addOneTimeListener("paused", () => {
      resumeDebuggerThenCloseAndFinish(gPanel).then(function () {
        ok(true, "should not throw after this point");
      });
    });

    callInTab(gTab, "debuggerThenThrow");
  });
}
