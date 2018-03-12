/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that `enable-framerate` toggles the visibility of the fps graph,
 * as well as enabling ticks data on the PerformanceFront.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { UI_ENABLE_FRAMERATE_PREF } = require("devtools/client/performance/test/helpers/prefs");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { isVisible } = require("devtools/client/performance/test/helpers/dom-utils");

add_task(function* () {
  let { panel } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { $, PerformanceController } = panel.panelWin;

  // Disable framerate to test.
  Services.prefs.setBoolPref(UI_ENABLE_FRAMERATE_PREF, false);

  yield startRecording(panel);
  yield stopRecording(panel);

  is(PerformanceController.getCurrentRecording().getConfiguration().withTicks, false,
    "PerformanceFront started without ticks recording.");
  ok(!isVisible($("#time-framerate")),
    "The fps graph is hidden when ticks disabled.");

  // Re-enable framerate.
  Services.prefs.setBoolPref(UI_ENABLE_FRAMERATE_PREF, true);

  is(PerformanceController.getCurrentRecording().getConfiguration().withTicks, false,
    "PerformanceFront still marked without ticks recording.");
  ok(!isVisible($("#time-framerate")),
    "The fps graph is still hidden if recording does not contain ticks.");

  yield startRecording(panel);
  yield stopRecording(panel);

  is(PerformanceController.getCurrentRecording().getConfiguration().withTicks, true,
    "PerformanceFront started with ticks recording.");
  ok(isVisible($("#time-framerate")),
    "The fps graph is not hidden when ticks enabled before recording.");

  yield teardownToolboxAndRemoveTab(panel);
});
