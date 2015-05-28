/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that events are fired from OverviewView from selection manipulation.
 */
function* spawnTest() {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController, OverviewView } = panel.panelWin;
  let startTime, endTime, params, _;

  yield startRecording(panel);

  yield Promise.all([
    once(OverviewView, EVENTS.FRAMERATE_GRAPH_RENDERED),
    once(OverviewView, EVENTS.MARKERS_GRAPH_RENDERED),
    once(OverviewView, EVENTS.OVERVIEW_RENDERED)
  ]);

  yield stopRecording(panel);

  let graph = OverviewView.graphs.get("timeline");
  let MAX = graph.width;

  // Select the first half of the graph
  let results = onceSpread(OverviewView, EVENTS.OVERVIEW_RANGE_SELECTED);
  dragStart(graph, 0);
  dragStop(graph, MAX / 2);
  [_, { startTime, endTime }] = yield results;

  let mapStart = () => 0;
  let mapEnd = () => PerformanceController.getCurrentRecording().getDuration();
  let actual = graph.getMappedSelection({ mapStart, mapEnd });
  is(graph.hasSelection(), true,
    "A selection exists on the graph.");
  is(startTime, actual.min,
    "OVERVIEW_RANGE_SELECTED fired with startTime value on click.");
  is(endTime, actual.max,
    "OVERVIEW_RANGE_SELECTED fired with endTime value on click.");

  // Listen to deselection
  results = onceSpread(OverviewView, EVENTS.OVERVIEW_RANGE_CLEARED);
  dropSelection(graph);
  [_, params] = yield results;

  is(graph.hasSelection(), false,
    "A selection no longer on the graph.");
  is(params, undefined,
    "OVERVIEW_RANGE_CLEARED fired with no additional arguments.");

  yield teardown(panel);
  finish();
}
