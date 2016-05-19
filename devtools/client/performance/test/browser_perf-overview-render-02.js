/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the overview graphs cannot be selected during recording
 * and that they're cleared upon rerecording.
 */

const { Constants } = require("devtools/client/performance/modules/constants");
const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { UI_ENABLE_MEMORY_PREF } = require("devtools/client/performance/test/helpers/prefs");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { times } = require("devtools/client/performance/test/helpers/event-utils");

add_task(function* () {
  let { panel } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { EVENTS, OverviewView } = panel.panelWin;

  // Enable memory to test.
  Services.prefs.setBoolPref(UI_ENABLE_MEMORY_PREF, true);

  yield startRecording(panel);

  let framerate = OverviewView.graphs.get("framerate");
  let markers = OverviewView.graphs.get("timeline");
  let memory = OverviewView.graphs.get("memory");

  ok("selectionEnabled" in framerate,
    "The selection should not be enabled for the framerate overview (1).");
  is(framerate.selectionEnabled, false,
    "The selection should not be enabled for the framerate overview (2).");
  is(framerate.hasSelection(), false,
    "The framerate overview shouldn't have a selection before recording.");

  ok("selectionEnabled" in markers,
    "The selection should not be enabled for the markers overview (1).");
  is(markers.selectionEnabled, false,
    "The selection should not be enabled for the markers overview (2).");
  is(markers.hasSelection(), false,
    "The markers overview shouldn't have a selection before recording.");

  ok("selectionEnabled" in memory,
    "The selection should not be enabled for the memory overview (1).");
  is(memory.selectionEnabled, false,
    "The selection should not be enabled for the memory overview (2).");
  is(memory.hasSelection(), false,
    "The memory overview shouldn't have a selection before recording.");

  // Ensure overview keeps rendering.
  yield times(OverviewView, EVENTS.UI_OVERVIEW_RENDERED, 3, {
    expectedArgs: { "1": Constants.FRAMERATE_GRAPH_LOW_RES_INTERVAL }
  });

  ok("selectionEnabled" in framerate,
    "The selection should still not be enabled for the framerate overview (1).");
  is(framerate.selectionEnabled, false,
    "The selection should still not be enabled for the framerate overview (2).");
  is(framerate.hasSelection(), false,
    "The framerate overview still shouldn't have a selection before recording.");

  ok("selectionEnabled" in markers,
    "The selection should still not be enabled for the markers overview (1).");
  is(markers.selectionEnabled, false,
    "The selection should still not be enabled for the markers overview (2).");
  is(markers.hasSelection(), false,
    "The markers overview still shouldn't have a selection before recording.");

  ok("selectionEnabled" in memory,
    "The selection should still not be enabled for the memory overview (1).");
  is(memory.selectionEnabled, false,
    "The selection should still not be enabled for the memory overview (2).");
  is(memory.hasSelection(), false,
    "The memory overview still shouldn't have a selection before recording.");

  yield stopRecording(panel);

  is(framerate.selectionEnabled, true,
    "The selection should now be enabled for the framerate overview.");
  is(markers.selectionEnabled, true,
    "The selection should now be enabled for the markers overview.");
  is(memory.selectionEnabled, true,
    "The selection should now be enabled for the memory overview.");

  yield teardownToolboxAndRemoveTab(panel);
});
