/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the overview graphs cannot be selected during recording
 * and that they're cleared upon rerecording.
 */
function* spawnTest() {
  // This test seems to take a long time to cleanup on Ubuntu VMs.
  requestLongerTimeout(2);

  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, OverviewView } = panel.panelWin;

  // Enable memory to test all the overview graphs.
  Services.prefs.setBoolPref(MEMORY_PREF, true);

  yield startRecording(panel);

  yield Promise.all([
    once(OverviewView, EVENTS.FRAMERATE_GRAPH_RENDERED),
    once(OverviewView, EVENTS.MARKERS_GRAPH_RENDERED),
    once(OverviewView, EVENTS.MEMORY_GRAPH_RENDERED),
    once(OverviewView, EVENTS.OVERVIEW_RENDERED),
  ]);

  let framerate = OverviewView.graphs.get("framerate");
  ok("selectionEnabled" in framerate,
    "The selection should not be enabled for the framerate overview (1).");
  is(framerate.selectionEnabled, false,
    "The selection should not be enabled for the framerate overview (2).");
  is(framerate.hasSelection(), false,
    "The framerate overview shouldn't have a selection before recording.");

  let markers = OverviewView.graphs.get("timeline");
  ok("selectionEnabled" in markers,
    "The selection should not be enabled for the markers overview (1).");
  is(markers.selectionEnabled, false,
    "The selection should not be enabled for the markers overview (2).");
  is(markers.hasSelection(), false,
    "The markers overview shouldn't have a selection before recording.");

  let memory = OverviewView.graphs.get("memory");
  ok("selectionEnabled" in memory,
    "The selection should not be enabled for the memory overview (1).");
  is(memory.selectionEnabled, false,
    "The selection should not be enabled for the memory overview (2).");
  is(memory.hasSelection(), false,
    "The memory overview shouldn't have a selection before recording.");

  let updated = 0;
  OverviewView.on(EVENTS.OVERVIEW_RENDERED, () => updated++);

  ok((yield waitUntil(() => updated > 10)),
    "The overviews were updated several times.");

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

  yield teardown(panel);
  finish();
}
