/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that events are fired from OverviewView from selection manipulation.
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, OverviewView } = panel.panelWin;
  let beginAt, endAt, params, _;

  yield startRecording(panel);

  yield once(OverviewView, EVENTS.OVERVIEW_RENDERED);

  yield stopRecording(panel);

  let graph = OverviewView.framerateGraph;
  let MAX = graph.width;

  // Select the first half of the graph
  let results = onceSpread(OverviewView, EVENTS.OVERVIEW_RANGE_SELECTED);
  dragStart(graph, 0);
  dragStop(graph, MAX / 2);

  [_, { beginAt, endAt }] = yield results;

  let actual = graph.getMappedSelection();
  ise(beginAt, actual.min, "OVERVIEW_RANGE_SELECTED fired with beginAt value on click.");
  ise(endAt, actual.max, "OVERVIEW_RANGE_SELECTED fired with endAt value on click.");

  // Listen to deselection
  results = onceSpread(OverviewView, EVENTS.OVERVIEW_RANGE_CLEARED);
  dropSelection(graph);
  [_, params] = yield results;

  is(graph.hasSelection(), false, "selection no longer on graph.");
  is(params, undefined, "OVERVIEW_RANGE_CLEARED fired with no additional arguments.");

  results = beginAt = endAt = graph = OverviewView = null;

  panel.panelWin.clearNamedTimeout("graph-scroll");
  yield teardown(panel);
  finish();
}
