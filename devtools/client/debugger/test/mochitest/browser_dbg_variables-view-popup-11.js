/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the watch expression button is added in variable view popup.
 */

const TAB_URL = EXAMPLE_URL + "doc_watch-expression-button.html";

function test() {
  Task.spawn(function* () {
    let options = {
      source: TAB_URL,
      line: 1
    };
    let [tab,, panel] = yield initDebugger(TAB_URL, options);
    let win = panel.panelWin;
    let events = win.EVENTS;
    let watch = win.DebuggerView.WatchExpressions;
    let bubble = win.DebuggerView.VariableBubble;
    let tooltip = bubble._tooltip.panel;

    let label = win.L10N.getStr("addWatchExpressionButton");
    let className = "dbg-expression-button";

    function testExpressionButton(aLabel, aClassName, aExpression) {
      ok(tooltip.querySelector("button"),
        "There should be a button available in variable view popup.");
      is(tooltip.querySelector("button").label, aLabel,
        "The button available is labeled correctly.");
      is(tooltip.querySelector("button").className, aClassName,
        "The button available is styled correctly.");

      tooltip.querySelector("button").click();

      ok(!tooltip.querySelector("button"),
        "There should be no button available in variable view popup.");
      ok(watch.getItemAtIndex(0),
        "The expression at index 0 should be available.");
      is(watch.getItemAtIndex(0).attachment.initialExpression, aExpression,
        "The expression at index 0 is correct.");
    }

    let onCaretAndScopes = waitForCaretAndScopes(panel, 19);
    callInTab(tab, "start");
    yield onCaretAndScopes;

    // Inspect primitive value variable.
    yield openVarPopup(panel, { line: 15, ch: 12 });
    let popupHiding = once(tooltip, "popuphiding");
    let expressionsEvaluated = waitForDebuggerEvents(panel, events.FETCHED_WATCH_EXPRESSIONS);
    testExpressionButton(label, className, "a");
    yield promise.all([popupHiding, expressionsEvaluated]);
    ok(true, "The new watch expressions were re-evaluated and the panel got hidden (1).");

    // Inspect non primitive value variable.
    expressionsEvaluated = waitForDebuggerEvents(panel, events.FETCHED_WATCH_EXPRESSIONS);
    yield openVarPopup(panel, { line: 16, ch: 12 }, true);
    yield expressionsEvaluated;
    ok(true, "The watch expressions were re-evaluated when a new panel opened (1).");

    popupHiding = once(tooltip, "popuphiding");
    expressionsEvaluated = waitForDebuggerEvents(panel, events.FETCHED_WATCH_EXPRESSIONS);
    testExpressionButton(label, className, "b");
    yield promise.all([popupHiding, expressionsEvaluated]);
    ok(true, "The new watch expressions were re-evaluated and the panel got hidden (2).");

    // Inspect property of an object.
    expressionsEvaluated = waitForDebuggerEvents(panel, events.FETCHED_WATCH_EXPRESSIONS);
    yield openVarPopup(panel, { line: 17, ch: 10 });
    yield expressionsEvaluated;
    ok(true, "The watch expressions were re-evaluated when a new panel opened (2).");

    popupHiding = once(tooltip, "popuphiding");
    expressionsEvaluated = waitForDebuggerEvents(panel, events.FETCHED_WATCH_EXPRESSIONS);
    testExpressionButton(label, className, "b.a");
    yield promise.all([popupHiding, expressionsEvaluated]);
    ok(true, "The new watch expressions were re-evaluated and the panel got hidden (3).");

    yield resumeDebuggerThenCloseAndFinish(panel);
  });
}
