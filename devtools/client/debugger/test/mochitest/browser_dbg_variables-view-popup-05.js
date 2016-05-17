/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests opening the variable inspection popup on a variable which has a
 * simple object as the value.
 */

const TAB_URL = EXAMPLE_URL + "doc_frame-parameters.html";

function test() {
  Task.spawn(function* () {
    let [tab,, panel] = yield initDebugger(TAB_URL);
    let win = panel.panelWin;
    let bubble = win.DebuggerView.VariableBubble;
    let tooltip = bubble._tooltip.panel;

    function verifyContents() {
      is(tooltip.querySelectorAll(".variables-view-container").length, 1,
        "There should be one variables view container added to the tooltip.");

      is(tooltip.querySelectorAll(".variables-view-scope[untitled]").length, 1,
        "There should be one scope with no header displayed.");
      is(tooltip.querySelectorAll(".variables-view-variable[untitled]").length, 1,
        "There should be one variable with no header displayed.");

      is(tooltip.querySelectorAll(".variables-view-property").length, 2,
        "There should be 2 properties displayed.");

      is(tooltip.querySelectorAll(".variables-view-property .name")[0].getAttribute("value"), "a",
        "The first property's name is correct.");
      is(tooltip.querySelectorAll(".variables-view-property .value")[0].getAttribute("value"), "1",
        "The first property's value is correct.");

      is(tooltip.querySelectorAll(".variables-view-property .name")[1].getAttribute("value"), "__proto__",
        "The second property's name is correct.");
      is(tooltip.querySelectorAll(".variables-view-property .value")[1].getAttribute("value"), "Object",
        "The second property's value is correct.");
    }

    callInTab(tab, "start");
    yield waitForSourceAndCaretAndScopes(panel, ".html", 24);

    // Inspect variable.
    yield openVarPopup(panel, { line: 16, ch: 12 }, true);
    verifyContents();

    yield resumeDebuggerThenCloseAndFinish(panel);
  });
}
