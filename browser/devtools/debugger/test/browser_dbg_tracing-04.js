/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that when we click on logs, we get the parameters/return value in the variables view.
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
        .then(clickTraceCall)
        .then(testParams)
        .then(clickTraceReturn)
        .then(testReturn)
        .then(() => stopTracing(gPanel))
        .then(() => {
          const deferred = promise.defer();
          SpecialPowers.popPrefEnv(deferred.resolve);
          return deferred.promise;
        })
        .then(() => closeDebuggerAndFinish(gPanel))
        .then(null, aError => {
          DevToolsUtils.reportException("browser_dbg_tracing-04.js", aError);
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

function clickTraceCall() {
  filterTraces(gPanel, t => t.querySelector(".trace-name[value=factorial]"))[0]
    .click();
}

function testParams() {
  const name = gDebugger.document.querySelector(".variables-view-variable .name");
  ok(name, "Should have a variable name");
  is(name.getAttribute("value"), "n", "The variable name should be n");

  const value = gDebugger.document.querySelector(".variables-view-variable .value.token-number");
  ok(value, "Should have a variable value");
  is(value.getAttribute("value"), "5", "The variable value should be 5");
}

function clickTraceReturn() {
  filterTraces(gPanel, t => t.querySelector(".trace-name[value=factorial]"))
    .pop().click();
}

function testReturn() {
  const name = gDebugger.document.querySelector(".variables-view-variable .name");
  ok(name, "Should have a variable name");
  is(name.getAttribute("value"), "<return>", "The variable name should be <return>");

  const value = gDebugger.document.querySelector(".variables-view-variable .value.token-number");
  ok(value, "Should have a variable value");
  is(value.getAttribute("value"), "120", "The variable value should be 120");
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
});
