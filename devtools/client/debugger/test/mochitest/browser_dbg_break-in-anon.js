/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure anonymous eval scripts can still break with a `debugger`
 * statement
 */

const TAB_URL = EXAMPLE_URL + "doc_script-eval.html";

function test() {
  const options = {
    source: EXAMPLE_URL + "code_script-eval.js",
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gSources = gDebugger.DebuggerView.Sources;

    return Task.spawn(function* () {
      is(gSources.values.length, 1, "Should have 1 source");

      callInTab(gTab, "evalSourceWithDebugger");
      yield waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN);

      is(gSources.values.length, 2, "Should have 2 sources");

      let item = gSources.getItemForAttachment(e => e.label.indexOf("SCRIPT") === 0);
      ok(item, "Source label is incorrect.");
      is(item.attachment.group, gDebugger.L10N.getStr("anonymousSourcesLabel"),
         "Source group is incorrect");

      yield resumeDebuggerThenCloseAndFinish(gPanel);
    });
  });
}
