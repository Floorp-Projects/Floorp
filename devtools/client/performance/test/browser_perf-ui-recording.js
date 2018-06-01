/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the controller handles recording via the `stopwatch` button
 * in the UI.
 */

const { pmmLoadFrameScripts, pmmIsProfilerActive, pmmClearFrameScripts } = require("devtools/client/performance/test/helpers/profiler-mm-utils");
const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");

add_task(async function() {
  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  pmmLoadFrameScripts(gBrowser);

  ok(!(await pmmIsProfilerActive()),
    "The built-in profiler module should not have been automatically started.");

  await startRecording(panel);

  ok((await pmmIsProfilerActive()),
    "The built-in profiler module should now be active.");

  await stopRecording(panel);

  ok((await pmmIsProfilerActive()),
    "The built-in profiler module should still be active.");

  await teardownToolboxAndRemoveTab(panel);

  pmmClearFrameScripts();
});
