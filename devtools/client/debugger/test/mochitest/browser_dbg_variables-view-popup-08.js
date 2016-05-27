/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests opening inspecting variables works across scopes.
 */

const TAB_URL = EXAMPLE_URL + "doc_scope-variable.html";

function test() {
  Task.spawn(function* () {
    let options = {
      source: TAB_URL,
      line: 1
    };
    let [tab,, panel] = yield initDebugger(TAB_URL, options);
    let win = panel.panelWin;
    let events = win.EVENTS;
    let editor = win.DebuggerView.editor;
    let frames = win.DebuggerView.StackFrames;
    let bubble = win.DebuggerView.VariableBubble;
    let tooltip = bubble._tooltip.panel;

    function verifyContents(textContent, className) {
      is(tooltip.querySelectorAll(".variables-view-container").length, 0,
        "There should be no variables view containers added to the tooltip.");
      is(tooltip.querySelectorAll(".devtools-tooltip-simple-text").length, 1,
        "There should be a simple text node added to the tooltip instead.");

      is(tooltip.querySelector(".devtools-tooltip-simple-text").textContent, textContent,
        "The inspected property's value is correct.");
      ok(tooltip.querySelector(".devtools-tooltip-simple-text").className.includes(className),
        "The inspected property's value is colorized correctly.");
    }

    function checkView(selectedFrame, caretLine) {
      is(win.gThreadClient.state, "paused",
        "Should only be getting stack frames while paused.");
      is(frames.itemCount, 2,
        "Should have two frames.");
      is(frames.selectedDepth, selectedFrame,
        "The correct frame is selected in the widget.");
      ok(isCaretPos(panel, caretLine),
        "Editor caret location is correct.");
    }

    let onCaretAndScopes = waitForCaretAndScopes(panel, 20);
    callInTab(tab, "test");
    yield onCaretAndScopes;

    checkView(0, 20);

    // Inspect variable in topmost frame.
    yield openVarPopup(panel, { line: 18, ch: 12 });
    verifyContents("\"second scope\"", "token-string");
    checkView(0, 20);

    // Hide the popup and change the frame.
    yield hideVarPopup(panel);

    let updatedFrame = waitForDebuggerEvents(panel, events.FETCHED_SCOPES);
    frames.selectedDepth = 1;
    yield updatedFrame;
    checkView(1, 15);

    // Inspect variable in oldest frame.
    yield openVarPopup(panel, { line: 13, ch: 12 });
    verifyContents("\"first scope\"", "token-string");
    checkView(1, 15);

    yield resumeDebuggerThenCloseAndFinish(panel);
  });
}
