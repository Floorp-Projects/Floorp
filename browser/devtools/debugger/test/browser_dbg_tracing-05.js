/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that text describing the tracing state is correctly displayed.
 */

const TAB_URL = EXAMPLE_URL + "doc_tracing-01.html";

let gTab, gDebuggee, gPanel, gDebugger;
let gTracer, gL10N;

function test() {
  SpecialPowers.pushPrefEnv({'set': [["devtools.debugger.tracer", true]]}, () => {
    initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
      gTab = aTab;
      gDebuggee = aDebuggee;
      gPanel = aPanel;
      gDebugger = gPanel.panelWin;
      gTracer = gDebugger.DebuggerView.Tracer;
      gL10N = gDebugger.L10N;

      waitForSourceShown(gPanel, "code_tracing-01.js")
        .then(testTracingNotStartedText)
        .then(() => gTracer._onStartTracing())
        .then(testFunctionCallsUnavailableText)
        .then(clickButton)
        .then(() => waitForClientEvents(aPanel, "traces"))
        .then(testNoEmptyText)
        .then(() => gTracer._onClear())
        .then(testFunctionCallsUnavailableText)
        .then(() => gTracer._onStopTracing())
        .then(testTracingNotStartedText)
        .then(() => gTracer._onClear())
        .then(testTracingNotStartedText)
        .then(() => {
          const deferred = promise.defer();
          SpecialPowers.popPrefEnv(deferred.resolve);
          return deferred.promise;
        })
        .then(() => closeDebuggerAndFinish(gPanel))
        .then(null, aError => {
          DevToolsUtils.reportException("browser_dbg_tracing-05.js", aError);
          ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
        });
    });
  });
}

function testTracingNotStartedText() {
  let label = gDebugger.document.querySelector("#tracer-tabpanel .fast-list-widget-empty-text");
  ok(label,
    "A label is displayed in the tracer tabpanel.");
  is(label.getAttribute("value"), gL10N.getStr("tracingNotStartedText"),
    "The correct {{tracingNotStartedText}} is displayed in the tracer tabpanel.");
}

function testFunctionCallsUnavailableText() {
  let label = gDebugger.document.querySelector("#tracer-tabpanel .fast-list-widget-empty-text");
  ok(label,
    "A label is displayed in the tracer tabpanel.");
  is(label.getAttribute("value"), gL10N.getStr("noFunctionCallsText"),
    "The correct {{noFunctionCallsText}} is displayed in the tracer tabpanel.");
}

function testNoEmptyText() {
  let label = gDebugger.document.querySelector("#tracer-tabpanel .fast-list-widget-empty-text");
  ok(!label,
    "No label should be displayed in the tracer tabpanel.");
}

function clickButton() {
  EventUtils.sendMouseEvent({ type: "click" },
                            gDebuggee.document.querySelector("button"),
                            gDebuggee);
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gTracer = null;
  gL10N = null;
});
