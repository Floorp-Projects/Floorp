/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests opening the variable inspection popup on a variable which has a
 * a property accessible via getters and setters.
 */

const TAB_URL = EXAMPLE_URL + "doc_frame-parameters.html";

function test() {
  Task.spawn(function* () {
    let [tab,, panel] = yield initDebugger(TAB_URL);
    let win = panel.panelWin;
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

    callInTab(tab, "start");
    yield waitForSourceAndCaretAndScopes(panel, ".html", 24);

    // Inspect properties.
    yield openVarPopup(panel, { line: 19, ch: 10 });
    verifyContents("42", "token-number");

    yield reopenVarPopup(panel, { line: 20, ch: 14 });
    verifyContents("42", "token-number");

    yield reopenVarPopup(panel, { line: 21, ch: 14 });
    verifyContents("42", "token-number");

    yield resumeDebuggerThenCloseAndFinish(panel);
  });
}
