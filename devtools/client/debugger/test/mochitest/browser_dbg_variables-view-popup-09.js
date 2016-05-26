/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests opening inspecting variables works across scopes.
 */

const TAB_URL = EXAMPLE_URL + "doc_scope-variable-3.html";

function test() {
  Task.spawn(function* () {
    let options = {
      source: TAB_URL,
      line: 1
    };
    let [tab,, panel] = yield initDebugger(TAB_URL, options);
    let win = panel.panelWin;
    let bubble = win.DebuggerView.VariableBubble;
    let tooltip = bubble._tooltip.panel;

    let onCaretAndScopes = waitForCaretAndScopes(panel, 15);
    callInTab(tab, "test");
    yield onCaretAndScopes;

    yield openVarPopup(panel, { line: 12, ch: 10 });
    ok(true, "The variable inspection popup was shown for the real variable.");

    once(tooltip, "popupshown").then(() => {
      ok(false, "The variable inspection popup shouldn't have been opened.");
    });

    reopenVarPopup(panel, { line: 18, ch: 10 });
    yield waitForTime(1000);

    yield resumeDebuggerThenCloseAndFinish(panel);
  });
}
