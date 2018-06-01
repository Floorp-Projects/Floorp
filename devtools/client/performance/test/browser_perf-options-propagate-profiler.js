/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that setting the `devtools.performance.profiler.` prefs propagate
 * to the profiler actor.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { PROFILER_BUFFER_SIZE_PREF, PROFILER_SAMPLE_RATE_PREF } = require("devtools/client/performance/test/helpers/prefs");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");

add_task(async function() {
  const { panel, toolbox } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  Services.prefs.setIntPref(PROFILER_BUFFER_SIZE_PREF, 1000);
  Services.prefs.setIntPref(PROFILER_SAMPLE_RATE_PREF, 2);

  await startRecording(panel);
  const { entries, interval } = await toolbox.performance.getConfiguration();
  await stopRecording(panel);

  is(entries, 1000, "profiler entries option is set on profiler");
  is(interval, 0.5, "profiler interval option is set on profiler");

  await teardownToolboxAndRemoveTab(panel);
});
