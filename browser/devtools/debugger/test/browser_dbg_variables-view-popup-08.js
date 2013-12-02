/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests opening inspecting variables works across scopes.
 */

const TAB_URL = EXAMPLE_URL + "doc_scope-variable.html";

function test() {
  Task.spawn(function() {
    let [tab, debuggee, panel] = yield initDebugger(TAB_URL);
    let win = panel.panelWin;
    let events = win.EVENTS;
    let editor = win.DebuggerView.editor;
    let frames = win.DebuggerView.StackFrames;
    let bubble = win.DebuggerView.VariableBubble;
    let tooltip = bubble._tooltip.panel;

    function openPopup(coords) {
      let popupshown = once(tooltip, "popupshown");
      let { left, top } = editor.getCoordsFromPosition(coords);
      bubble._findIdentifier(left, top);
      return popupshown;
    }

    function hidePopup() {
      let popuphiding = once(tooltip, "popuphiding");
      bubble.hideContents();
      return popuphiding.then(waitForTick);
    }

    function verifyContents(textContent, className) {
      is(tooltip.querySelectorAll(".variables-view-container").length, 0,
        "There should be no variables view containers added to the tooltip.");
      is(tooltip.querySelectorAll(".devtools-tooltip-simple-text").length, 1,
        "There should be a simple text node added to the tooltip instead.");

      is(tooltip.querySelector(".devtools-tooltip-simple-text").textContent, textContent,
        "The inspected property's value is correct.");
      ok(tooltip.querySelector(".devtools-tooltip-simple-text").className.contains(className),
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

    // Allow this generator function to yield first.
    executeSoon(() => debuggee.test());
    yield waitForSourceAndCaretAndScopes(panel, ".html", 20);
    checkView(0, 20);

    // Inspect variable in topmost frame.
    yield openPopup({ line: 18, ch: 12 });
    verifyContents("\"second scope\"", "token-string");
    checkView(0, 20);

    // Change frame.
    let updatedFrame = waitForDebuggerEvents(panel, events.FETCHED_SCOPES);
    yield hidePopup().then(() => frames.selectedDepth = 1);
    yield updatedFrame;
    checkView(1, 15);

    // Inspect variable in oldest frame.
    yield openPopup({ line: 13, ch: 12 });
    verifyContents("\"first scope\"", "token-string");
    checkView(1, 15);

    yield resumeDebuggerThenCloseAndFinish(panel);
  });
}
