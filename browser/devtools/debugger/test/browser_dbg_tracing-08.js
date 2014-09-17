/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that tracing about:config doesn't produce errors.
 */

const TAB_URL = "about:config";

let gPanel, gDoneChecks;

function test() {
  gDoneChecks = promise.defer();
  const tracerPref = promise.defer();
  const configPref = promise.defer();
  SpecialPowers.pushPrefEnv({'set': [["devtools.debugger.tracer", true]]}, tracerPref.resolve);
  SpecialPowers.pushPrefEnv({'set': [["general.warnOnAboutConfig", false]]}, configPref.resolve);
  promise.all([tracerPref.promise, configPref.promise]).then(() => {
    initDebugger(TAB_URL).then(([,, aPanel]) => {
      gPanel = aPanel;
      gPanel.panelWin.gClient.addOneTimeListener("traces", testTraceLogs);
    }).then(() => startTracing(gPanel))
      .then(generateTrace)
      .then(() => waitForClientEvents(gPanel, "traces"))
      .then(() => gDoneChecks.promise)
      .then(() => stopTracing(gPanel))
      .then(resetPreferences)
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function testTraceLogs(name, packet) {
  info("Traces: " + packet.traces.length);
  ok(packet.traces.length > 0, "Got some traces.");
  ok(packet.traces.every(t => t.type != "enteredFrame" || !!t.location),
     "All enteredFrame traces contain location.");
  gDoneChecks.resolve();
}

function generateTrace(name, packet) {
  // Interact with the page to cause JS execution.
  let search = content.document.getElementById("textbox");
  info("Interacting with the page.");
  search.value = "devtools";
}

function resetPreferences() {
  const deferred = promise.defer();
  SpecialPowers.popPrefEnv(() => SpecialPowers.popPrefEnv(deferred.resolve));
  return deferred.promise;
}

registerCleanupFunction(function() {
  gPanel = null;
  gDoneChecks = null;
});
