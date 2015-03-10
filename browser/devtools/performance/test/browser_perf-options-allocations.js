/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */


/**
 * Tests that setting the `devtools.performance.memory.` prefs propagate to the memory actor.
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController, $, gFront } = panel.panelWin;
  Services.prefs.setBoolPref(MEMORY_PREF, true);

  let originalProbability = Services.prefs.getCharPref("devtools.performance.memory.sample-probability");
  let originalLogLength = Services.prefs.getIntPref("devtools.performance.memory.max-log-length");

  Services.prefs.setCharPref("devtools.performance.memory.sample-probability", "0.213");
  Services.prefs.setIntPref("devtools.performance.memory.max-log-length", 777777);

  yield startRecording(panel);

  let { probability, maxLogLength } = yield gFront._request("memory", "getAllocationsSettings");

  yield stopRecording(panel);

  is(probability, 0.213, "allocations probability option is set on memory actor");
  is(maxLogLength, 777777, "allocations max log length option is set on memory actor");

  Services.prefs.setBoolPref(MEMORY_PREF, false);
  Services.prefs.setCharPref("devtools.performance.memory.sample-probability", originalProbability);
  Services.prefs.setIntPref("devtools.performance.memory.max-log-length", originalLogLength);
  yield teardown(panel);
  finish();
}
