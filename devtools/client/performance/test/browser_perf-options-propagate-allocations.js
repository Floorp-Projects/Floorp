/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that setting the `devtools.performance.memory.` prefs propagate to
 * the memory actor.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const {
  MEMORY_SAMPLE_PROB_PREF,
  MEMORY_MAX_LOG_LEN_PREF,
  UI_ENABLE_ALLOCATIONS_PREF,
} = require("devtools/client/performance/test/helpers/prefs");
const {
  initPerformanceInNewTab,
  teardownToolboxAndRemoveTab,
} = require("devtools/client/performance/test/helpers/panel-utils");
const {
  startRecording,
  stopRecording,
} = require("devtools/client/performance/test/helpers/actions");

add_task(async function() {
  const { panel, toolbox } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window,
  });

  // Enable allocations to test.
  Services.prefs.setBoolPref(UI_ENABLE_ALLOCATIONS_PREF, true);
  Services.prefs.setCharPref(MEMORY_SAMPLE_PROB_PREF, "0.213");
  Services.prefs.setIntPref(MEMORY_MAX_LOG_LEN_PREF, 777777);

  await startRecording(panel);
  const performanceFront = await toolbox.target.getFront("performance");
  const {
    probability,
    maxLogLength,
  } = await performanceFront.getConfiguration();
  await stopRecording(panel);

  is(
    probability,
    0.213,
    "The allocations probability option is set on memory actor."
  );
  is(
    maxLogLength,
    777777,
    "The allocations max log length option is set on memory actor."
  );

  await teardownToolboxAndRemoveTab(panel);
});
