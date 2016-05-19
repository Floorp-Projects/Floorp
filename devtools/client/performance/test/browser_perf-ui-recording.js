/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the controller handles recording via the `stopwatch` button
 * in the UI.
 */

const { PMM_loadFrameScripts, PMM_isProfilerActive, PMM_clearFrameScripts } = require("devtools/client/performance/test/helpers/profiler-mm-utils");
const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");

add_task(function* () {
  let { panel } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  PMM_loadFrameScripts(gBrowser);

  ok(!(yield PMM_isProfilerActive()),
    "The built-in profiler module should not have been automatically started.");

  yield startRecording(panel);

  ok((yield PMM_isProfilerActive()),
    "The built-in profiler module should now be active.");

  yield stopRecording(panel);

  ok((yield PMM_isProfilerActive()),
    "The built-in profiler module should still be active.");

  yield teardownToolboxAndRemoveTab(panel);

  PMM_clearFrameScripts();
});
