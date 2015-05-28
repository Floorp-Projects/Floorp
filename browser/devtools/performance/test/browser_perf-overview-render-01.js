/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the overview continuously renders content when recording.
 */
function* spawnTest() {
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

  yield Promise.all([
    once(OverviewView, EVENTS.FRAMERATE_GRAPH_RENDERED),
    once(OverviewView, EVENTS.MARKERS_GRAPH_RENDERED),
    once(OverviewView, EVENTS.MEMORY_GRAPH_RENDERED),
    once(OverviewView, EVENTS.OVERVIEW_RENDERED),
  ]);

  yield stopRecording(panel);

  yield teardown(panel);
  finish();
}
