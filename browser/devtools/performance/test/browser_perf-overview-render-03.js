/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the overview graphs share the exact same width and scaling.
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController, OverviewView } = panel.panelWin;

  // Enable memory to test all the overview graphs.
  Services.prefs.setBoolPref(MEMORY_PREF, true);

  yield startRecording(panel);

  let updated = 0;
  OverviewView.on(EVENTS.OVERVIEW_RENDERED, () => updated++);

  yield busyWait(100);
  yield waitUntil(() => PerformanceController.getCurrentRecording().getMarkers().length);
  yield waitUntil(() => PerformanceController.getCurrentRecording().getMemory().length);
  yield waitUntil(() => PerformanceController.getCurrentRecording().getTicks().length);
  yield waitUntil(() => updated > 10);

  yield stopRecording(panel);

  let markers = OverviewView.graphs.get("timeline");
  let framerate = OverviewView.graphs.get("framerate");
  let memory = OverviewView.graphs.get("memory");

  ok(markers.width > 0,
    "The overview's markers graph has a width.");
  ok(markers.dataScaleX > 0,
    "The overview's markers graph has a data scale factor.");

  ok(memory.width > 0,
    "The overview's memory graph has a width.");
  ok(memory.dataDuration > 0,
    "The overview's memory graph has a data duration.");
  ok(memory.dataScaleX > 0,
    "The overview's memory graph has a data scale factor.");

  ok(framerate.width > 0,
    "The overview's framerate graph has a width.");
  ok(framerate.dataDuration > 0,
    "The overview's framerate graph has a data duration.");
  ok(framerate.dataScaleX > 0,
    "The overview's framerate graph has a data scale factor.");

  is(markers.width, memory.width,
    "The markers and memory graphs widths are the same.");
  is(markers.width, framerate.width,
    "The markers and framerate graphs widths are the same.");

  is(memory.dataDuration, framerate.dataDuration,
    "The memory and framerate graphs data duration are the same.");

  is(markers.dataScaleX, memory.dataScaleX,
    "The markers and memory graphs data scale are the same.");
  is(markers.dataScaleX, framerate.dataScaleX,
    "The markers and framerate graphs data scale are the same.");

  yield teardown(panel);
  finish();
}
