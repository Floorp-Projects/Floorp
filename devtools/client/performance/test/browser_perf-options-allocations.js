/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */


/**
 * Tests that setting the `devtools.performance.memory.` prefs propagate to the memory actor.
 */
function* spawnTest() {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController, $, gFront } = panel.panelWin;
  Services.prefs.setBoolPref(ALLOCATIONS_PREF, true);

  let originalProbability = Services.prefs.getCharPref(MEMORY_SAMPLE_PROB_PREF);
  let originalLogLength = Services.prefs.getIntPref(MEMORY_MAX_LOG_LEN_PREF);

  Services.prefs.setCharPref(MEMORY_SAMPLE_PROB_PREF, "0.213");
  Services.prefs.setIntPref(MEMORY_MAX_LOG_LEN_PREF, 777777);

  yield startRecording(panel);

  let { probability, maxLogLength } = yield gFront.getConfiguration();

  yield stopRecording(panel);

  is(probability, 0.213, "allocations probability option is set on memory actor");
  is(maxLogLength, 777777, "allocations max log length option is set on memory actor");

  Services.prefs.setBoolPref(ALLOCATIONS_PREF, false);
  Services.prefs.setCharPref(MEMORY_SAMPLE_PROB_PREF, originalProbability);
  Services.prefs.setIntPref(MEMORY_MAX_LOG_LEN_PREF, originalLogLength);
  yield teardown(panel);
  finish();
}
