/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Execute code both before and after blackboxing and test that we get
 * appropriately styled traces.
 */

const TAB_URL = EXAMPLE_URL + "doc_tracing-01.html";

let gTab, gDebuggee, gPanel;

function test() {
  Task.async(function*() {
    yield pushPref();

    [gTab, gDebuggee, gPanel] = yield initDebugger(TAB_URL);

    yield startTracing(gPanel);
    yield clickButton();
    yield waitForClientEvents(gPanel, "traces");

    /**
     * Test that there are some traces which are not blackboxed.
     */
    const firstBbButton = getBlackBoxButton(gPanel);
    ok(!firstBbButton.checked, "Should not be black boxed by default");

    const blackBoxedTraces =
      gPanel.panelWin.document.querySelectorAll(".trace-item.black-boxed");
    ok(blackBoxedTraces.length === 0, "There should no blackboxed traces.");

    const notBlackBoxedTraces =
      gPanel.panelWin.document.querySelectorAll(".trace-item:not(.black-boxed)");
    ok(notBlackBoxedTraces.length > 0,
      "There should be some traces which are not blackboxed.");

    yield toggleBlackBoxing(gPanel);
    yield clickButton();
    yield waitForClientEvents(gPanel, "traces");

    /**
     * Test that there are some traces which are blackboxed.
     */
    const secondBbButton = getBlackBoxButton(gPanel);
    ok(secondBbButton.checked, "The checkbox should no longer be checked.");
    const traces =
      gPanel.panelWin.document.querySelectorAll(".trace-item.black-boxed");
    ok(traces.length > 0, "There should be some blackboxed traces.");

    yield stopTracing(gPanel);
    yield popPref();
    yield closeDebuggerAndFinish(gPanel);

    finish();
  })().catch(e => {
    ok(false, "Got an error: " + e.message + "\n" + e.stack);
    finish();
  });
}

function clickButton() {
  EventUtils.sendMouseEvent({ type: "click" },
                            gDebuggee.document.querySelector("button"),
                            gDebuggee);
}

function pushPref() {
  let deferred = promise.defer();
  SpecialPowers.pushPrefEnv({'set': [["devtools.debugger.tracer", true]]},
    deferred.resolve);
  return deferred.promise;
}

function popPref() {
  let deferred = promise.defer();
  SpecialPowers.popPrefEnv(deferred.resolve);
  return deferred.promise;
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
});

