/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the variable inspection popup behaves correctly when switching
 * between simple and complex objects.
 */

const TAB_URL = EXAMPLE_URL + "doc_frame-parameters.html";

function test() {
  Task.spawn(function() {
    let [tab, debuggee, panel] = yield initDebugger(TAB_URL);
    let win = panel.panelWin;
    let bubble = win.DebuggerView.VariableBubble;
    let tooltip = bubble._tooltip.panel;

    function verifySimpleContents(textContent, className) {
      is(tooltip.querySelectorAll(".variables-view-container").length, 0,
        "There should be no variables view container added to the tooltip.");
      is(tooltip.querySelectorAll(".devtools-tooltip-simple-text").length, 1,
        "There should be one simple text node added to the tooltip.");

      is(tooltip.querySelector(".devtools-tooltip-simple-text").textContent, textContent,
        "The inspected property's value is correct.");
      ok(tooltip.querySelector(".devtools-tooltip-simple-text").className.contains(className),
        "The inspected property's value is colorized correctly.");
    }

    function verifyComplexContents(propertyCount) {
      is(tooltip.querySelectorAll(".variables-view-container").length, 1,
        "There should be one variables view container added to the tooltip.");
      is(tooltip.querySelectorAll(".devtools-tooltip-simple-text").length, 0,
        "There should be no simple text node added to the tooltip.");

      is(tooltip.querySelectorAll(".variables-view-scope[non-header]").length, 1,
        "There should be one scope with no header displayed.");
      is(tooltip.querySelectorAll(".variables-view-variable[non-header]").length, 1,
        "There should be one variable with no header displayed.");

      ok(tooltip.querySelectorAll(".variables-view-property").length >= propertyCount,
        "There should be some properties displayed.");
    }

    // Allow this generator function to yield first.
    executeSoon(() => debuggee.start());
    yield waitForSourceAndCaretAndScopes(panel, ".html", 24);

    // Inspect variables.
    yield openVarPopup(panel, { line: 15, ch: 12 });
    verifySimpleContents("1", "token-number");

    yield reopenVarPopup(panel, { line: 16, ch: 12 }, true);
    verifyComplexContents(2);

    yield reopenVarPopup(panel, { line: 19, ch: 10 });
    verifySimpleContents("42", "token-number");

    yield reopenVarPopup(panel, { line: 31, ch: 10 }, true);
    verifyComplexContents(100);

    yield resumeDebuggerThenCloseAndFinish(panel);
  });
}
