/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that selections in the flame graph widget work properly on HiDPI.

const TEST_DATA = [
  {
    color: "#f00",
    blocks: [
      { x: 0, y: 0, width: 50, height: 20, text: "FOO" },
      { x: 50, y: 0, width: 100, height: 20, text: "BAR" },
    ],
  },
  {
    color: "#00f",
    blocks: [{ x: 0, y: 30, width: 30, height: 20, text: "BAZ" }],
  },
];
const TEST_BOUNDS = { startTime: 0, endTime: 150 };
const TEST_WIDTH = 200;
const TEST_HEIGHT = 100;
const TEST_DPI_DENSITIY = 2;

var { FlameGraph } = require("devtools/client/shared/widgets/FlameGraph");

add_task(async function() {
  await addTab("about:blank");
  await performTest();
  gBrowser.removeCurrentTab();
});

async function performTest() {
  const { host, doc } = await createHost();
  doc.body.setAttribute(
    "style",
    "position: fixed; width: 100%; height: 100%; margin: 0;"
  );

  const graph = new FlameGraph(doc.body, TEST_DPI_DENSITIY);
  graph.fixedWidth = TEST_WIDTH;
  graph.fixedHeight = TEST_HEIGHT;

  await graph.ready();

  testGraph(graph);

  await graph.destroy();
  host.destroy();
}

function testGraph(graph) {
  graph.setData({ data: TEST_DATA, bounds: TEST_BOUNDS });

  is(
    graph.getViewRange().startTime,
    0,
    "The selection start boundary is correct on HiDPI (1)."
  );
  is(
    graph.getViewRange().endTime,
    150,
    "The selection end boundary is correct on HiDPI (1)."
  );

  is(
    graph.getOuterBounds().startTime,
    0,
    "The bounds start boundary is correct on HiDPI (1)."
  );
  is(
    graph.getOuterBounds().endTime,
    150,
    "The bounds end boundary is correct on HiDPI (1)."
  );

  scroll(graph, 10000, HORIZONTAL_AXIS, 1);

  is(
    Math.round(graph.getViewRange().startTime),
    150,
    "The selection start boundary is correct on HiDPI (2)."
  );
  is(
    Math.round(graph.getViewRange().endTime),
    150,
    "The selection end boundary is correct on HiDPI (2)."
  );

  is(
    graph.getOuterBounds().startTime,
    0,
    "The bounds start boundary is correct on HiDPI (2)."
  );
  is(
    graph.getOuterBounds().endTime,
    150,
    "The bounds end boundary is correct on HiDPI (2)."
  );
}

// EventUtils just doesn't work!

var HORIZONTAL_AXIS = 1;
var VERTICAL_AXIS = 2;

function scroll(graph, wheel, axis, x, y = 1) {
  x /= window.devicePixelRatio;
  y /= window.devicePixelRatio;
  graph._onMouseMove({ testX: x, testY: y });
  graph._onMouseWheel({
    testX: x,
    testY: y,
    axis,
    detail: wheel,
    HORIZONTAL_AXIS,
    VERTICAL_AXIS,
  });
}
