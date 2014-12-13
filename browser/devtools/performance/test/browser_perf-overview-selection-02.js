/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the graphs' selection is correctly disabled or enabled.
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, OverviewView } = panel.panelWin;
  let framerateGraph = OverviewView.framerateGraph;
  let markersOverview = OverviewView.markersOverview;
  let memoryOverview = OverviewView.memoryOverview;

  yield startRecording(panel);

  ok(!framerateGraph.selectionEnabled,
    "Selection shouldn't be enabled when the first recording started (1).");
  ok(!markersOverview.selectionEnabled,
    "Selection shouldn't be enabled when the first recording started (2).");
  ok(!memoryOverview.selectionEnabled,
    "Selection shouldn't be enabled when the first recording started (3).");

  yield stopRecording(panel);

  ok(framerateGraph.selectionEnabled,
    "Selection should be enabled when the first recording finishes (1).");
  ok(markersOverview.selectionEnabled,
    "Selection should be enabled when the first recording finishes (2).");
  ok(memoryOverview.selectionEnabled,
    "Selection should be enabled when the first recording finishes (3).");

  yield startRecording(panel);

  ok(!framerateGraph.selectionEnabled,
    "Selection shouldn't be enabled when the second recording started (1).");
  ok(!markersOverview.selectionEnabled,
    "Selection shouldn't be enabled when the second recording started (2).");
  ok(!memoryOverview.selectionEnabled,
    "Selection shouldn't be enabled when the second recording started (3).");

  yield stopRecording(panel);

  ok(framerateGraph.selectionEnabled,
    "Selection should be enabled when the first second finishes (1).");
  ok(markersOverview.selectionEnabled,
    "Selection should be enabled when the first second finishes (2).");
  ok(memoryOverview.selectionEnabled,
    "Selection should be enabled when the first second finishes (3).");

  yield teardown(panel);
  finish();
}
