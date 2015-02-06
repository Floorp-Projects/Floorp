/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the overview graphs cannot be selected during recording
 * and that they're cleared upon rerecording.
 */
function spawnTest () {
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

  ok("selectionEnabled" in OverviewView.framerateGraph,
    "The selection should not be enabled for the framerate overview (1).");
  is(OverviewView.framerateGraph.selectionEnabled, false,
    "The selection should not be enabled for the framerate overview (2).");
  is(OverviewView.framerateGraph.hasSelection(), false,
    "The framerate overview shouldn't have a selection before recording.");

  ok("selectionEnabled" in OverviewView.markersOverview,
    "The selection should not be enabled for the markers overview (1).");
  is(OverviewView.markersOverview.selectionEnabled, false,
    "The selection should not be enabled for the markers overview (2).");
  is(OverviewView.markersOverview.hasSelection(), false,
    "The markers overview shouldn't have a selection before recording.");

  ok("selectionEnabled" in OverviewView.memoryOverview,
    "The selection should not be enabled for the memory overview (1).");
  is(OverviewView.memoryOverview.selectionEnabled, false,
    "The selection should not be enabled for the memory overview (2).");
  is(OverviewView.memoryOverview.hasSelection(), false,
    "The memory overview shouldn't have a selection before recording.");

  let updated = 0;
  OverviewView.on(EVENTS.OVERVIEW_RENDERED, () => updated++);

  ok((yield waitUntil(() => updated > 10)),
    "The overviews were updated several times.");

  ok("selectionEnabled" in OverviewView.framerateGraph,
    "The selection should still not be enabled for the framerate overview (1).");
  is(OverviewView.framerateGraph.selectionEnabled, false,
    "The selection should still not be enabled for the framerate overview (2).");
  is(OverviewView.framerateGraph.hasSelection(), false,
    "The framerate overview still shouldn't have a selection before recording.");

  ok("selectionEnabled" in OverviewView.markersOverview,
    "The selection should still not be enabled for the markers overview (1).");
  is(OverviewView.markersOverview.selectionEnabled, false,
    "The selection should still not be enabled for the markers overview (2).");
  is(OverviewView.markersOverview.hasSelection(), false,
    "The markers overview still shouldn't have a selection before recording.");

  ok("selectionEnabled" in OverviewView.memoryOverview,
    "The selection should still not be enabled for the memory overview (1).");
  is(OverviewView.memoryOverview.selectionEnabled, false,
    "The selection should still not be enabled for the memory overview (2).");
  is(OverviewView.memoryOverview.hasSelection(), false,
    "The memory overview still shouldn't have a selection before recording.");

  yield stopRecording(panel);

  is(OverviewView.framerateGraph.selectionEnabled, true,
    "The selection should now be enabled for the framerate overview.");

  is(OverviewView.markersOverview.selectionEnabled, true,
    "The selection should now be enabled for the markers overview.");

  is(OverviewView.memoryOverview.selectionEnabled, true,
    "The selection should now be enabled for the memory overview.");

  yield teardown(panel);
  finish();
}
