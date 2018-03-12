/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that toggling `enable-memory` during a recording doesn't change that
 * recording's state and does not break.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { UI_ENABLE_FRAMERATE_PREF } = require("devtools/client/performance/test/helpers/prefs");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");

add_task(function* () {
  let { panel } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { PerformanceController } = panel.panelWin;

  // Test starting without framerate, and stopping with it.
  Services.prefs.setBoolPref(UI_ENABLE_FRAMERATE_PREF, false);
  yield startRecording(panel);

  Services.prefs.setBoolPref(UI_ENABLE_FRAMERATE_PREF, true);
  yield stopRecording(panel);

  is(PerformanceController.getCurrentRecording().getConfiguration().withTicks, false,
    "The recording finished without tracking framerate.");

  // Test starting with framerate, and stopping without it.
  yield startRecording(panel);

  Services.prefs.setBoolPref(UI_ENABLE_FRAMERATE_PREF, false);
  yield stopRecording(panel);

  is(PerformanceController.getCurrentRecording().getConfiguration().withTicks, true,
    "The recording finished with tracking framerate.");

  yield teardownToolboxAndRemoveTab(panel);
});
