/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

requestLongerTimeout(2);

/**
 * Tests that events are fired from OverviewView from selection manipulation.
 */
function* spawnTest() {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController, OverviewView } = panel.panelWin;
  let params, _;

  yield startRecording(panel);

  yield Promise.all([
    once(OverviewView, EVENTS.FRAMERATE_GRAPH_RENDERED),
    once(OverviewView, EVENTS.MARKERS_GRAPH_RENDERED),
    once(OverviewView, EVENTS.OVERVIEW_RENDERED)
  ]);

  yield stopRecording(panel);

  let graph = OverviewView.graphs.get("timeline");
  let MAX = graph.width;
  let duration = PerformanceController.getCurrentRecording().getDuration();
  let selection = null;

  // Throw out events that select everything, as this will occur on the
  // first click
  OverviewView.on(EVENTS.OVERVIEW_RANGE_SELECTED, function handler (_, interval) {
    if (interval.endTime !== duration) {
      selection = interval;
      OverviewView.off(handler);
    }
  });

  let results = onceSpread(OverviewView, EVENTS.OVERVIEW_RANGE_SELECTED);
  // Select the first half of the graph
  dragStart(graph, 0);
  dragStop(graph, MAX / 2);
  yield waitUntil(() => selection);
  let { startTime, endTime } = selection;

  let mapStart = () => 0;
  let mapEnd = () => duration;
  let actual = graph.getMappedSelection({ mapStart, mapEnd });
  is(actual.min, 0, "graph selection starts at 0");
  is(actual.max, duration/2, `graph selection ends at ${duration/2}`);

  is(graph.hasSelection(), true,
    "A selection exists on the graph.");
  is(startTime, 0,
    "OVERVIEW_RANGE_SELECTED fired with startTime value on click.");
  is(endTime, duration / 2,
    "OVERVIEW_RANGE_SELECTED fired with endTime value on click.");

  // Listen to deselection
  results = onceSpread(OverviewView, EVENTS.OVERVIEW_RANGE_SELECTED);
  dropSelection(graph);
  [_, params] = yield results;

  is(graph.hasSelection(), false,
    "A selection no longer on the graph.");
  is(params.startTime, 0,
    "OVERVIEW_RANGE_SELECTED fired with 0 as a startTime.");
  is(params.endTime, duration,
    "OVERVIEW_RANGE_SELECTED fired with max duration as endTime");

  yield teardown(panel);
  finish();
}
