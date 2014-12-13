/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the graphs' selections are linked.
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, OverviewView } = panel.panelWin;
  let framerateGraph = OverviewView.framerateGraph;
  let markersOverview = OverviewView.markersOverview;
  let memoryOverview = OverviewView.memoryOverview;

  let MAX = framerateGraph.width;

  yield startRecording(panel);
  yield stopRecording(panel);

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
