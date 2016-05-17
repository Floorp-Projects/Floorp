/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the clicking "Watch" button twice, for the same expression, only adds it
 * once.
 */

const TAB_URL = EXAMPLE_URL + "doc_watch-expression-button.html";

function test() {
  Task.spawn(function* () {
    let [tab,, panel] = yield initDebugger(TAB_URL);
    let win = panel.panelWin;
    let events = win.EVENTS;
    let watch = win.DebuggerView.WatchExpressions;
    let bubble = win.DebuggerView.VariableBubble;
    let tooltip = bubble._tooltip.panel;

    function verifyContent(aExpression, aItemCount) {

      ok(watch.getItemAtIndex(0),
        "The expression at index 0 should be available.");
      is(watch.getItemAtIndex(0).attachment.initialExpression, aExpression,
        "The expression at index 0 is correct.");
      is(watch.itemCount, aItemCount,
        "The expression count is correct.");
    }

    callInTab(tab, "start");
    yield waitForSourceAndCaretAndScopes(panel, ".html", 19);

    // Inspect primitive value variable.
    yield openVarPopup(panel, { line: 15, ch: 12 });
    let popupHiding = once(tooltip, "popuphiding");
    let expressionsEvaluated = waitForDebuggerEvents(panel, events.FETCHED_WATCH_EXPRESSIONS);
    tooltip.querySelector("button").click();
    verifyContent("a", 1);
    yield promise.all([popupHiding, expressionsEvaluated]);
    ok(true, "The new watch expressions were re-evaluated and the panel got hidden (1).");

    // Inspect property of an object.
    expressionsEvaluated = waitForDebuggerEvents(panel, events.FETCHED_WATCH_EXPRESSIONS);
    yield openVarPopup(panel, { line: 17, ch: 10 });
    yield expressionsEvaluated;
    ok(true, "The watch expressions were re-evaluated when a new panel opened (1).");

    popupHiding = once(tooltip, "popuphiding");
    expressionsEvaluated = waitForDebuggerEvents(panel, events.FETCHED_WATCH_EXPRESSIONS);
    tooltip.querySelector("button").click();
    verifyContent("b.a", 2);
    yield promise.all([popupHiding, expressionsEvaluated]);
    ok(true, "The new watch expressions were re-evaluated and the panel got hidden (2).");

    // Re-inspect primitive value variable.
    expressionsEvaluated = waitForDebuggerEvents(panel, events.FETCHED_WATCH_EXPRESSIONS);
    yield openVarPopup(panel, { line: 15, ch: 12 });
    yield expressionsEvaluated;
    ok(true, "The watch expressions were re-evaluated when a new panel opened (2).");

    popupHiding = once(tooltip, "popuphiding");
    expressionsEvaluated = waitForDebuggerEvents(panel, events.FETCHED_WATCH_EXPRESSIONS);
    tooltip.querySelector("button").click();
    verifyContent("b.a", 2);
    yield promise.all([popupHiding, expressionsEvaluated]);
    ok(true, "The new watch expressions were re-evaluated and the panel got hidden (3).");

    yield resumeDebuggerThenCloseAndFinish(panel);
  });
}
