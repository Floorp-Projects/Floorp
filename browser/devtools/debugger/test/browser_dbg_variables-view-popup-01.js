/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests opening the variable inspection popup on a variable which has a
 * simple literal as the value.
 */

const TAB_URL = EXAMPLE_URL + "doc_frame-parameters.html";

function test() {
  Task.spawn(function() {
    let [tab, debuggee, panel] = yield initDebugger(TAB_URL);
    let win = panel.panelWin;
    let events = win.EVENTS;
    let editor = win.DebuggerView.editor;
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

    // Allow this generator function to yield first.
    executeSoon(() => debuggee.start());
    yield waitForSourceAndCaretAndScopes(panel, ".html", 24);

    // Inspect variables.
    yield openPopup({ line: 15, ch: 12 });
    verifyContents("1", "token-number");

    yield hidePopup().then(() => openPopup({ line: 16, ch: 21 }));
    verifyContents("1", "token-number");

    yield hidePopup().then(() => openPopup({ line: 17, ch: 21 }));
    verifyContents("1", "token-number");

    yield hidePopup().then(() => openPopup({ line: 17, ch: 27 }));
    verifyContents("\"beta\"", "token-string");

    yield hidePopup().then(() => openPopup({ line: 17, ch: 44 }));
    verifyContents("false", "token-boolean");

    yield hidePopup().then(() => openPopup({ line: 17, ch: 54 }));
    verifyContents("null", "token-null");

    yield hidePopup().then(() => openPopup({ line: 17, ch: 63 }));
    verifyContents("undefined", "token-undefined");

    yield resumeDebuggerThenCloseAndFinish(panel);
  });
}
