/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the overview graphs do not render when realtime rendering is off
 * due to lack of e10s.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { UI_ENABLE_MEMORY_PREF } = require("devtools/client/performance/test/helpers/prefs");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { waitUntil } = require("devtools/client/performance/test/helpers/wait-utils");
const { isVisible } = require("devtools/client/performance/test/helpers/dom-utils");

add_task(function* () {
  let { panel } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { $, EVENTS, PerformanceController, RecordingsView, OverviewView } = panel.panelWin;

  // Enable memory to test.
  Services.prefs.setBoolPref(UI_ENABLE_MEMORY_PREF, true);

  // Set realtime rendering off.
  OverviewView.isRealtimeRenderingEnabled = () => false;

  let updated = 0;
  OverviewView.on(EVENTS.UI_OVERVIEW_RENDERED, () => updated++);

  yield startRecording(panel, { skipWaitingForOverview: true });

  is(isVisible($("#overview-pane")), false, "Overview graphs hidden.");
  is(updated, 0, "Overview graphs have not been updated");

  yield waitUntil(() => PerformanceController.getCurrentRecording().getMarkers().length);
  yield waitUntil(() => PerformanceController.getCurrentRecording().getMemory().length);
  yield waitUntil(() => PerformanceController.getCurrentRecording().getTicks().length);
  is(isVisible($("#overview-pane")), false, "Overview graphs still hidden.");
  is(updated, 0, "Overview graphs have still not been updated");

  yield stopRecording(panel);

  is(isVisible($("#overview-pane")), true, "Overview graphs no longer hidden.");
  is(updated, 1, "Overview graphs rendered upon completion.");

  yield startRecording(panel, { skipWaitingForOverview: true });

  is(isVisible($("#overview-pane")), false,
     "Overview graphs hidden again when starting new recording.");
  is(updated, 1, "Overview graphs have not been updated again.");

  RecordingsView.selectedIndex = 0;
  is(isVisible($("#overview-pane")), true,
     "Overview graphs no longer hidden when switching back to complete recording.");
  is(updated, 1, "Overview graphs have not been updated again.");

  RecordingsView.selectedIndex = 1;
  is(isVisible($("#overview-pane")), false,
     "Overview graphs hidden again when going back to inprogress recording.");
  is(updated, 1, "Overview graphs have not been updated again.");

  yield stopRecording(panel);

  is(isVisible($("#overview-pane")), true,
     "overview graphs no longer hidden when recording finishes");
  is(updated, 2, "Overview graphs rendered again upon completion.");

  yield teardownToolboxAndRemoveTab(panel);
});
