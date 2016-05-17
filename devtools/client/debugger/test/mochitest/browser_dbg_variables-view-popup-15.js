/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests opening the variable inspection popup directly on literals.
 */

const TAB_URL = EXAMPLE_URL + "doc_frame-parameters.html";

function test() {
  Task.spawn(function* () {
    let [tab,, panel] = yield initDebugger(TAB_URL);
    let win = panel.panelWin;
    let bubble = win.DebuggerView.VariableBubble;
    let tooltip = bubble._tooltip.panel;

    callInTab(tab, "start");
    yield waitForSourceAndCaretAndScopes(panel, ".html", 24);

    yield openVarPopup(panel, { line: 15, ch: 12 });
    ok(true, "The variable inspection popup was shown for the real variable.");

    once(tooltip, "popupshown").then(() => {
      ok(false, "The variable inspection popup shouldn't have been opened.");
    });

    reopenVarPopup(panel, { line: 17, ch: 27 });
    yield waitForTime(1000);

    yield resumeDebuggerThenCloseAndFinish(panel);
  });
}
