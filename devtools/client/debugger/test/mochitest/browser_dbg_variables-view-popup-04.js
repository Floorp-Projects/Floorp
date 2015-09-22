/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the variable inspection popup is hidden when the editor scrolls.
 */

const TAB_URL = EXAMPLE_URL + "doc_frame-parameters.html";

function test() {
  Task.spawn(function*() {
    let [tab,, panel] = yield initDebugger(TAB_URL);
    let win = panel.panelWin;
    let bubble = win.DebuggerView.VariableBubble;

    callInTab(tab, "start");
    yield waitForSourceAndCaretAndScopes(panel, ".html", 24);

    // Inspect variable.
    yield openVarPopup(panel, { line: 15, ch: 12 });
    yield hideVarPopupByScrollingEditor(panel);
    ok(true, "The variable inspection popup was hidden.");

    ok(bubble._tooltip.isEmpty(),
      "The variable inspection popup is now empty.");
    ok(!bubble._markedText,
      "The marked text in the editor was removed.");

    yield resumeDebuggerThenCloseAndFinish(panel);
  });
}
