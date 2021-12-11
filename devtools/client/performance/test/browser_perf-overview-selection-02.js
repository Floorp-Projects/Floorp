/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the graphs' selection is correctly disabled or enabled.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const {
  UI_ENABLE_MEMORY_PREF,
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
  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window,
  });

  const { OverviewView } = panel.panelWin;

  // Enable memory to test.
  Services.prefs.setBoolPref(UI_ENABLE_MEMORY_PREF, true);

  await startRecording(panel);

  const markersOverview = OverviewView.graphs.get("timeline");
  const memoryGraph = OverviewView.graphs.get("memory");
  const framerateGraph = OverviewView.graphs.get("framerate");

  ok(markersOverview, "The markers graph should have been created now.");
  ok(memoryGraph, "The memory graph should have been created now.");
  ok(framerateGraph, "The framerate graph should have been created now.");

  ok(
    !markersOverview.selectionEnabled,
    "Selection shouldn't be enabled when the first recording started (2)."
  );
  ok(
    !memoryGraph.selectionEnabled,
    "Selection shouldn't be enabled when the first recording started (3)."
  );
  ok(
    !framerateGraph.selectionEnabled,
    "Selection shouldn't be enabled when the first recording started (1)."
  );

  await stopRecording(panel);

  ok(
    markersOverview.selectionEnabled,
    "Selection should be enabled when the first recording finishes (2)."
  );
  ok(
    memoryGraph.selectionEnabled,
    "Selection should be enabled when the first recording finishes (3)."
  );
  ok(
    framerateGraph.selectionEnabled,
    "Selection should be enabled when the first recording finishes (1)."
  );

  await startRecording(panel);

  ok(
    !markersOverview.selectionEnabled,
    "Selection shouldn't be enabled when the second recording started (2)."
  );
  ok(
    !memoryGraph.selectionEnabled,
    "Selection shouldn't be enabled when the second recording started (3)."
  );
  ok(
    !framerateGraph.selectionEnabled,
    "Selection shouldn't be enabled when the second recording started (1)."
  );

  await stopRecording(panel);

  ok(
    markersOverview.selectionEnabled,
    "Selection should be enabled when the first second finishes (2)."
  );
  ok(
    memoryGraph.selectionEnabled,
    "Selection should be enabled when the first second finishes (3)."
  );
  ok(
    framerateGraph.selectionEnabled,
    "Selection should be enabled when the first second finishes (1)."
  );

  await teardownToolboxAndRemoveTab(panel);
});
