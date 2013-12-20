/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we highlight matching calls and returns on hover.
 */

const TAB_URL = EXAMPLE_URL + "doc_tracing-01.html";

let gTab, gDebuggee, gPanel, gDebugger;

function test() {
  SpecialPowers.pushPrefEnv({'set': [["devtools.debugger.tracer", true]]}, () => {
    initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
      gTab = aTab;
      gDebuggee = aDebuggee;
      gPanel = aPanel;
      gDebugger = gPanel.panelWin;

      waitForSourceShown(gPanel, "code_tracing-01.js")
        .then(() => startTracing(gPanel))
        .then(clickButton)
        .then(() => waitForClientEvents(aPanel, "traces"))
        .then(highlightCall)
        .then(testReturnHighlighted)
        .then(unhighlightCall)
        .then(testNoneHighlighted)
        .then(() => stopTracing(gPanel))
        .then(() => {
          const deferred = promise.defer();
          SpecialPowers.popPrefEnv(deferred.resolve);
          return deferred.promise;
        })
        .then(() => closeDebuggerAndFinish(gPanel))
        .then(null, aError => {
          ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
        });
    });
  });
}

function clickButton() {
  EventUtils.sendMouseEvent({ type: "click" },
                            gDebuggee.document.querySelector("button"),
                            gDebuggee);
}

function highlightCall() {
  const callTrace = filterTraces(gPanel, t => t.querySelector(".trace-name[value=main]"))[0];
  EventUtils.sendMouseEvent({ type: "mouseover" },
                            callTrace,
                            gDebugger);
}

function testReturnHighlighted() {
  const returnTrace = filterTraces(gPanel, t => t.querySelector(".trace-name[value=main]"))[1];
  ok(Array.indexOf(returnTrace.querySelector(".trace-item").classList, "selected-matching") >= 0,
     "The corresponding return log should be highlighted.");
}

function unhighlightCall() {
  const callTrace = filterTraces(gPanel, t => t.querySelector(".trace-name[value=main]"))[0];
  EventUtils.sendMouseEvent({ type: "mouseout" },
                            callTrace,
                            gDebugger);
}

function testNoneHighlighted() {
  const highlightedTraces = filterTraces(gPanel, t => t.querySelector(".selected-matching"));
  is(highlightedTraces.length, 0, "Shouldn't have any highlighted traces");
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
});
