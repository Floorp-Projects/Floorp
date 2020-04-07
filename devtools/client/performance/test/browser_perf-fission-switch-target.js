/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Test behavior while target-switching.
 */

const {
  MAIN_PROCESS_URL,
  SIMPLE_URL: CONTENT_PROCESS_URL,
} = require("devtools/client/performance/test/helpers/urls");
const {
  BrowserTestUtils,
} = require("resource://testing-common/BrowserTestUtils.jsm");
const {
  addTab,
} = require("devtools/client/performance/test/helpers/tab-utils");
const {
  initPerformanceInTab,
  teardownToolboxAndRemoveTab,
} = require("devtools/client/performance/test/helpers/panel-utils");
const {
  startRecording,
  stopRecording,
} = require("devtools/client/performance/test/helpers/actions");

add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["devtools.target-switching.enabled", true]],
  });

  info("Open a page running on content process");
  const tab = await addTab({
    url: CONTENT_PROCESS_URL,
    win: window,
  });

  info("Open the performance panel");
  const { panel } = await initPerformanceInTab({ tab });
  const { PerformanceController, PerformanceView, EVENTS } = panel.panelWin;

  info("Start recording");
  await startRecording(panel);
  await PerformanceView.once(EVENTS.UI_RECORDING_PROFILER_STATUS_RENDERED);

  info("Navigate to a page running on main process");
  await BrowserTestUtils.loadURI(tab.linkedBrowser, MAIN_PROCESS_URL);
  await PerformanceView.once(EVENTS.UI_RECORDING_PROFILER_STATUS_RENDERED);

  info("Return to a page running on content process again");
  await BrowserTestUtils.loadURI(tab.linkedBrowser, CONTENT_PROCESS_URL);
  await PerformanceView.once(EVENTS.UI_RECORDING_PROFILER_STATUS_RENDERED);

  info("Stop recording");
  await stopRecording(panel);

  const recordings = PerformanceController.getRecordings();
  is(recordings.length, 3, "Have a record for every target-switching");

  await teardownToolboxAndRemoveTab(panel);
});
