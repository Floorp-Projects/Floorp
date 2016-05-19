/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that events are fired from selection manipulation.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { once } = require("devtools/client/performance/test/helpers/event-utils");
const { idleWait } = require("devtools/client/performance/test/helpers/wait-utils");
const { dragStartCanvasGraph, dragStopCanvasGraph, clickCanvasGraph } = require("devtools/client/performance/test/helpers/input-utils");

add_task(function* () {
  let { panel } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { EVENTS, PerformanceController, OverviewView } = panel.panelWin;

  yield startRecording(panel);
  yield stopRecording(panel);

  let duration = PerformanceController.getCurrentRecording().getDuration();
  let graph = OverviewView.graphs.get("timeline");

  // Select the first half of the graph.

  let rangeSelected = once(OverviewView, EVENTS.UI_OVERVIEW_RANGE_SELECTED, { spreadArgs: true });
  dragStartCanvasGraph(graph, { x: 0 });
  let [_, { startTime, endTime }] = yield rangeSelected;
  is(endTime, duration, "The selected range is the entire graph, for now.");

  rangeSelected = once(OverviewView, EVENTS.UI_OVERVIEW_RANGE_SELECTED, { spreadArgs: true });
  dragStopCanvasGraph(graph, { x: graph.width / 2 });
  [_, { startTime, endTime }] = yield rangeSelected;
  is(endTime, duration / 2, "The selected range is half of the graph.");

  is(graph.hasSelection(), true,
    "A selection exists on the graph.");
  is(startTime, 0,
    "The UI_OVERVIEW_RANGE_SELECTED event fired with 0 as a `startTime`.");
  is(endTime, duration / 2,
    `The UI_OVERVIEW_RANGE_SELECTED event fired with ${duration / 2} as \`endTime\`.`);

  let mapStart = () => 0;
  let mapEnd = () => duration;
  let actual = graph.getMappedSelection({ mapStart, mapEnd });
  is(actual.min, 0, "Graph selection starts at 0.");
  is(actual.max, duration / 2, `Graph selection ends at ${duration / 2}.`);

  // Listen to deselection.

  rangeSelected = once(OverviewView, EVENTS.UI_OVERVIEW_RANGE_SELECTED, { spreadArgs: true });
  clickCanvasGraph(graph, { x: 3 * graph.width / 4 });
  [_, { startTime, endTime }] = yield rangeSelected;

  is(graph.hasSelection(), false,
    "A selection no longer on the graph.");
  is(startTime, 0,
    "The UI_OVERVIEW_RANGE_SELECTED event fired with 0 as a `startTime`.");
  is(endTime, duration,
    "The UI_OVERVIEW_RANGE_SELECTED event fired with duration as `endTime`.");

  yield teardownToolboxAndRemoveTab(panel);
});
