/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the overview graphs share the exact same width and scaling.
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController, OverviewView } = panel.panelWin;

  yield startRecording(panel);

  let updated = 0;
  OverviewView.on(EVENTS.OVERVIEW_RENDERED, () => updated++);

  yield busyWait(100);
  yield waitUntil(() => PerformanceController.getCurrentRecording().getMarkers().length);
  yield waitUntil(() => PerformanceController.getCurrentRecording().getMemory().length);
  yield waitUntil(() => PerformanceController.getCurrentRecording().getTicks().length);
  yield waitUntil(() => updated > 10);

  yield stopRecording(panel);

  // Wait for the overview graph to be rerendered *after* recording.
  yield once(OverviewView, EVENTS.OVERVIEW_RENDERED);

  ok(OverviewView.markersOverview.width > 0,
    "The overview's framerate graph has a width.");
  ok(OverviewView.markersOverview.dataScaleX > 0,
    "The overview's framerate graph has a data scale factor.");

  ok(OverviewView.memoryOverview.width > 0,
    "The overview's framerate graph has a width.");
  ok(OverviewView.memoryOverview.dataDuration > 0,
    "The overview's framerate graph has a data duration.");
  ok(OverviewView.memoryOverview.dataScaleX > 0,
    "The overview's framerate graph has a data scale factor.");

  ok(OverviewView.framerateGraph.width > 0,
    "The overview's framerate graph has a width.");
  ok(OverviewView.framerateGraph.dataDuration > 0,
    "The overview's framerate graph has a data duration.");
  ok(OverviewView.framerateGraph.dataScaleX > 0,
    "The overview's framerate graph has a data scale factor.");

  is(OverviewView.markersOverview.width,
     OverviewView.memoryOverview.width,
    "The markers and memory graphs widths are the same.")
  is(OverviewView.markersOverview.width,
     OverviewView.framerateGraph.width,
    "The markers and framerate graphs widths are the same.");

  is(OverviewView.memoryOverview.dataDuration,
     OverviewView.framerateGraph.dataDuration,
    "The memory and framerate graphs data duration are the same.");

  is(OverviewView.markersOverview.dataScaleX,
     OverviewView.memoryOverview.dataScaleX,
    "The markers and memory graphs data scale are the same.")
  is(OverviewView.markersOverview.dataScaleX,
     OverviewView.framerateGraph.dataScaleX,
    "The markers and framerate graphs data scale are the same.");

  yield teardown(panel);
  finish();
}
