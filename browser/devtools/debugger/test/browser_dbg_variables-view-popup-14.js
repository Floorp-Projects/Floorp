/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the variable inspection popup is hidden when
 * selecting text in the editor.
 */

const TAB_URL = EXAMPLE_URL + "doc_frame-parameters.html";

function test() {
  Task.spawn(function*() {
    let [tab, debuggee, panel] = yield initDebugger(TAB_URL);
    let win = panel.panelWin;
    let bubble = win.DebuggerView.VariableBubble;

    // Allow this generator function to yield first.
    executeSoon(() => debuggee.start());
    yield waitForSourceAndCaretAndScopes(panel, ".html", 24);

    // Select some text.
    let cursor = win.DebuggerView.editor.getOffset({ line: 15, ch: 12 });
    let [ anchor, head ] = win.DebuggerView.editor.getPosition(
      cursor,
      cursor + 3
    );
    win.DebuggerView.editor.setSelection(anchor, head);

    // Try to Inspect variable during selection.
    let popupOpened = yield intendOpenVarPopup(panel, { line: 15, ch: 12 }, true);

    // Ensure the bubble is not there
    ok(!popupOpened,
      "The popup is not opened");
    ok(!bubble._markedText,
      "The marked text in the editor is not there.");

    // Try to Inspect variable after selection.
    popupOpened = yield intendOpenVarPopup(panel, { line: 15, ch: 12 }, false);

    // Ensure the bubble is not there
    ok(popupOpened,
      "The popup is opened");
    ok(bubble._markedText,
      "The marked text in the editor is there.");

    yield resumeDebuggerThenCloseAndFinish(panel);
  });
}
