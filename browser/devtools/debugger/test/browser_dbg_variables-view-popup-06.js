/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests opening the variable inspection popup on a variable which has a
 * complext object as the value.
 */

const TAB_URL = EXAMPLE_URL + "doc_frame-parameters.html";

function test() {
  Task.spawn(function() {
    let [tab, debuggee, panel] = yield initDebugger(TAB_URL);
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

      is(tooltip.querySelectorAll(".variables-view-property").length, 7,
        "There should be 7 properties displayed.");

      is(tooltip.querySelectorAll(".variables-view-property .name")[0].getAttribute("value"), "a",
        "The first property's name is correct.");
      is(tooltip.querySelectorAll(".variables-view-property .value")[0].getAttribute("value"), "1",
        "The first property's value is correct.");

      is(tooltip.querySelectorAll(".variables-view-property .name")[1].getAttribute("value"), "b",
        "The second property's name is correct.");
      is(tooltip.querySelectorAll(".variables-view-property .value")[1].getAttribute("value"), "\"beta\"",
        "The second property's value is correct.");

      is(tooltip.querySelectorAll(".variables-view-property .name")[2].getAttribute("value"), "c",
        "The third property's name is correct.");
      is(tooltip.querySelectorAll(".variables-view-property .value")[2].getAttribute("value"), "3",
        "The third property's value is correct.");

      is(tooltip.querySelectorAll(".variables-view-property .name")[3].getAttribute("value"), "d",
        "The fourth property's name is correct.");
      is(tooltip.querySelectorAll(".variables-view-property .value")[3].getAttribute("value"), "false",
        "The fourth property's value is correct.");

      is(tooltip.querySelectorAll(".variables-view-property .name")[4].getAttribute("value"), "e",
        "The fifth property's name is correct.");
      is(tooltip.querySelectorAll(".variables-view-property .value")[4].getAttribute("value"), "null",
        "The fifth property's value is correct.");

      is(tooltip.querySelectorAll(".variables-view-property .name")[5].getAttribute("value"), "f",
        "The sixth property's name is correct.");
      is(tooltip.querySelectorAll(".variables-view-property .value")[5].getAttribute("value"), "undefined",
        "The sixth property's value is correct.");

      is(tooltip.querySelectorAll(".variables-view-property .name")[6].getAttribute("value"), "__proto__",
        "The seventh property's name is correct.");
      is(tooltip.querySelectorAll(".variables-view-property .value")[6].getAttribute("value"), "Object",
        "The seventh property's value is correct.");
    }

    // Allow this generator function to yield first.
    executeSoon(() => debuggee.start());
    yield waitForSourceAndCaretAndScopes(panel, ".html", 24);

    // Inspect variable.
    yield openVarPopup(panel, { line: 17, ch: 12 }, true);
    verifyContents();

    yield resumeDebuggerThenCloseAndFinish(panel);
  });
}
