/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the graphs' selections are linked.
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

  yield stopRecording(panel);

  let framerateGraph = OverviewView.graphs.get("framerate");
  let markersOverview = OverviewView.graphs.get("timeline");
  let memoryOverview = OverviewView.graphs.get("memory");
  let MAX = framerateGraph.width;

  // Perform a selection inside the framerate graph.

  let selected = once(OverviewView, EVENTS.OVERVIEW_RANGE_SELECTED);
  dragStart(framerateGraph, 0);
  dragStop(framerateGraph, MAX / 2);
  yield selected;

  is(framerateGraph.getSelection().toSource(), "({start:0, end:" + (MAX / 2) + "})",
    "The framerate graph has a correct selection.");
  is(markersOverview.getSelection().toSource(), framerateGraph.getSelection().toSource(),
    "The markers overview has a correct selection.");
  is(memoryOverview.getSelection().toSource(), framerateGraph.getSelection().toSource(),
    "The memory overview has a correct selection.");

  // Perform a selection inside the markers overview.

  selected = once(OverviewView, EVENTS.OVERVIEW_RANGE_SELECTED);
  markersOverview.dropSelection();
  dragStart(markersOverview, 0);
  dragStop(markersOverview, MAX / 4);
  yield selected;

  is(framerateGraph.getSelection().toSource(), "({start:0, end:" + (MAX / 4) + "})",
    "The framerate graph has a correct selection.");
  is(markersOverview.getSelection().toSource(), framerateGraph.getSelection().toSource(),
    "The markers overview has a correct selection.");
  is(memoryOverview.getSelection().toSource(), framerateGraph.getSelection().toSource(),
    "The memory overview has a correct selection.");

  // Perform a selection inside the memory overview.

  selected = once(OverviewView, EVENTS.OVERVIEW_RANGE_SELECTED);
  memoryOverview.dropSelection();
  dragStart(memoryOverview, 0);
  dragStop(memoryOverview, MAX / 10);
  yield selected;

  is(framerateGraph.getSelection().toSource(), "({start:0, end:" + (MAX / 10) + "})",
    "The framerate graph has a correct selection.");
  is(markersOverview.getSelection().toSource(), framerateGraph.getSelection().toSource(),
    "The markers overview has a correct selection.");
  is(memoryOverview.getSelection().toSource(), framerateGraph.getSelection().toSource(),
    "The memory overview has a correct selection.");

  yield teardown(panel);
  finish();
}
