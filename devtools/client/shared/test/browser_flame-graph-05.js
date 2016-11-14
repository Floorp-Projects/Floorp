/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that flame graph widget has proper keyboard support.

const TEST_DATA = [
  {
    color: "#f00",
    blocks: [
      { x: 0, y: 0, width: 50, height: 20, text: "FOO" },
      { x: 50, y: 0, width: 100, height: 20, text: "BAR" }
    ]
  },
  {
    color: "#00f",
    blocks: [
      { x: 0, y: 30, width: 30, height: 20, text: "BAZ" }
    ]
  }
];
const TEST_BOUNDS = { startTime: 0, endTime: 150 };
const TEST_DPI_DENSITIY = 2;

const KEY_CODE_UP = 38;
const KEY_CODE_LEFT = 37;
const KEY_CODE_RIGHT = 39;

var {FlameGraph} = require("devtools/client/shared/widgets/FlameGraph");

add_task(function* () {
  yield addTab("about:blank");
  yield performTest();
  gBrowser.removeCurrentTab();
});

function* performTest() {
  let [host,, doc] = yield createHost();
  doc.body.setAttribute("style",
                        "position: fixed; width: 100%; height: 100%; margin: 0;");

  let graph = new FlameGraph(doc.body, TEST_DPI_DENSITIY);
  yield graph.ready();

  yield testGraph(host, graph);

  yield graph.destroy();
  host.destroy();
}

function* testGraph(host, graph) {
  graph.setData({ data: TEST_DATA, bounds: TEST_BOUNDS });

  is(graph._selection.start, 0,
    "The graph's selection start value is initially correct.");
  is(graph._selection.end, TEST_BOUNDS.endTime * TEST_DPI_DENSITIY,
    "The graph's selection end value is initially correct.");

  yield pressKeyForTime(graph, KEY_CODE_LEFT, 1000);

  is(graph._selection.start, 0,
    "The graph's selection start value is correct after pressing LEFT.");
  ok(graph._selection.end < TEST_BOUNDS.endTime * TEST_DPI_DENSITIY,
    "The graph's selection end value is correct after pressing LEFT.");

  graph._selection.start = 0;
  graph._selection.end = TEST_BOUNDS.endTime * TEST_DPI_DENSITIY;
  info("Graph selection was reset (1).");

  yield pressKeyForTime(graph, KEY_CODE_RIGHT, 1000);

  ok(graph._selection.start > 0,
    "The graph's selection start value is correct after pressing RIGHT.");
  is(graph._selection.end, TEST_BOUNDS.endTime * TEST_DPI_DENSITIY,
    "The graph's selection end value is correct after pressing RIGHT.");

  graph._selection.start = 0;
  graph._selection.end = TEST_BOUNDS.endTime * TEST_DPI_DENSITIY;
  info("Graph selection was reset (2).");

  yield pressKeyForTime(graph, KEY_CODE_UP, 1000);

  ok(graph._selection.start > 0,
    "The graph's selection start value is correct after pressing UP.");
  ok(graph._selection.end < TEST_BOUNDS.endTime * TEST_DPI_DENSITIY,
    "The graph's selection end value is correct after pressing UP.");

  let distanceLeft = graph._selection.start;
  let distanceRight = TEST_BOUNDS.endTime * TEST_DPI_DENSITIY - graph._selection.end;

  ok(Math.abs(distanceRight - distanceLeft) < 0.1,
    "The graph zoomed correctly towards the center point.");
}

function pressKeyForTime(graph, keyCode, ms) {
  graph._onKeyDown({
    keyCode,
    preventDefault: () => {},
    stopPropagation: () => {},
  });

  return new Promise(resolve => {
    setTimeout(() => {
      graph._onKeyUp({
        keyCode,
        preventDefault: () => {},
        stopPropagation: () => {},
      });
      resolve();
    }, ms);
  });
}
