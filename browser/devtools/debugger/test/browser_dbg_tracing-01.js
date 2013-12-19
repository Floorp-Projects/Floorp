/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we get the expected frame enter/exit logs in the tracer view.
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
        .then(testTraceLogs)
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

function testTraceLogs() {
  const onclickLogs = filterTraces(gPanel,
                                   t => t.querySelector(".trace-name[value=onclick]"));
  is(onclickLogs.length, 2, "Should have two logs from 'onclick'");
  ok(onclickLogs[0].querySelector(".trace-call"),
     "The first 'onclick' log should be a call.");
  ok(onclickLogs[1].querySelector(".trace-return"),
     "The second 'onclick' log should be a return.");
  for (let t of onclickLogs) {
    ok(t.querySelector(".trace-item").getAttribute("tooltiptext")
        .contains("doc_tracing-01.html"));
  }

  const nonOnclickLogs = filterTraces(gPanel,
                                      t => !t.querySelector(".trace-name[value=onclick]"));
  for (let t of nonOnclickLogs) {
    ok(t.querySelector(".trace-item").getAttribute("tooltiptext")
        .contains("code_tracing-01.js"));
  }

  const mainLogs = filterTraces(gPanel,
                                t => t.querySelector(".trace-name[value=main]"));
  is(mainLogs.length, 2, "Should have an enter and an exit for 'main'");
  ok(mainLogs[0].querySelector(".trace-call"),
     "The first 'main' log should be a call.");
  ok(mainLogs[1].querySelector(".trace-return"),
     "The second 'main' log should be a return.");

  const factorialLogs = filterTraces(gPanel,
                                     t => t.querySelector(".trace-name[value=factorial]"));
  is(factorialLogs.length, 10, "Should have 5 enter, and 5 exit frames for 'factorial'");
  ok(factorialLogs.slice(0, 5).every(t => t.querySelector(".trace-call")),
     "The first five 'factorial' logs should be calls.");
  ok(factorialLogs.slice(5).every(t => t.querySelector(".trace-return")),
     "The second five 'factorial' logs should be returns.")

  // Test that the depth affects padding so that calls are indented properly.
  let lastDepth = -Infinity;
  for (let t of factorialLogs.slice(0, 5)) {
    let depth = parseInt(t.querySelector(".trace-item").style.MozPaddingStart, 10);
    ok(depth > lastDepth, "The depth should be increasing");
    lastDepth = depth;
  }
  lastDepth = Infinity;
  for (let t of factorialLogs.slice(5)) {
    let depth = parseInt(t.querySelector(".trace-item").style.MozPaddingStart, 10);
    ok(depth < lastDepth, "The depth should be decreasing");
    lastDepth = depth;
  }

  const throwerLogs = filterTraces(gPanel,
                                   t => t.querySelector(".trace-name[value=thrower]"));
  is(throwerLogs.length, 2, "Should have an enter and an exit for 'thrower'");
  ok(throwerLogs[0].querySelector(".trace-call"),
     "The first 'thrower' log should be a call.");
  ok(throwerLogs[1].querySelector(".trace-throw",
     "The second 'thrower' log should be a throw."));
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
});
