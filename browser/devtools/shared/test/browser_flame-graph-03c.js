/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that vertical panning in the flame graph widget works properly.

let TEST_DATA = [{ color: "#f00", blocks: [{ x: 0, y: 0, width: 50, height: 20, text: "FOO" }, { x: 50, y: 0, width: 100, height: 20, text: "BAR" }] }, { color: "#00f", blocks: [{ x: 0, y: 30, width: 30, height: 20, text: "BAZ" }] }];
let TEST_BOUNDS = { startTime: 0, endTime: 150 };
let TEST_WIDTH = 200;
let TEST_HEIGHT = 100;
let TEST_DPI_DENSITIY = 2;

let {FlameGraph} = devtools.require("devtools/shared/widgets/FlameGraph");
let {Promise} = devtools.require("resource://gre/modules/Promise.jsm");

add_task(function*() {
  yield promiseTab("about:blank");
  yield performTest();
  gBrowser.removeCurrentTab();
});

function* performTest() {
  let [host, win, doc] = yield createHost();
  doc.body.setAttribute("style", "position: fixed; width: 100%; height: 100%; margin: 0;");

  let graph = new FlameGraph(doc.body, TEST_DPI_DENSITIY);
  graph.fixedWidth = TEST_WIDTH;
  graph.fixedHeight = TEST_HEIGHT;

  yield graph.ready();

  testGraph(graph);

  yield graph.destroy();
  host.destroy();
}

function testGraph(graph) {
  graph.setData({ data: TEST_DATA, bounds: TEST_BOUNDS });

  // Drag up vertically only.

  dragStart(graph, TEST_WIDTH / 2, TEST_HEIGHT / 2);
  is(graph.getViewRange().startTime | 0, 0,
    "The selection start boundary is correct (1).");
  is(graph.getViewRange().endTime | 0, 150,
    "The selection end boundary is correct (1).");
  is(graph.getViewRange().verticalOffset | 0, 0,
    "The vertical offset is correct (1).");

  hover(graph, TEST_WIDTH / 2, TEST_HEIGHT / 2 - 50);
  is(graph.getViewRange().startTime | 0, 0,
    "The selection start boundary is correct (2).");
  is(graph.getViewRange().endTime | 0, 150,
    "The selection end boundary is correct (2).");
  is(graph.getViewRange().verticalOffset | 0, 17,
    "The vertical offset is correct (2).");

  dragStop(graph, TEST_WIDTH / 2, TEST_HEIGHT / 2 - 100);
  is(graph.getViewRange().startTime | 0, 0,
    "The selection start boundary is correct (3).");
  is(graph.getViewRange().endTime | 0, 150,
    "The selection end boundary is correct (3).");
  is(graph.getViewRange().verticalOffset | 0, 42,
    "The vertical offset is correct (3).");

  // Drag down strongly vertically and slightly horizontally.

  dragStart(graph, TEST_WIDTH / 2, TEST_HEIGHT / 2);
  is(graph.getViewRange().startTime | 0, 0,
    "The selection start boundary is correct (4).");
  is(graph.getViewRange().endTime | 0, 150,
    "The selection end boundary is correct (4).");
  is(graph.getViewRange().verticalOffset | 0, 42,
    "The vertical offset is correct (4).");

  hover(graph, TEST_WIDTH / 2, TEST_HEIGHT / 2 + 50);
  is(graph.getViewRange().startTime | 0, 0,
    "The selection start boundary is correct (5).");
  is(graph.getViewRange().endTime | 0, 150,
    "The selection end boundary is correct (5).");
  is(graph.getViewRange().verticalOffset | 0, 25,
    "The vertical offset is correct (5).");

  dragStop(graph, TEST_WIDTH / 2 + 100, TEST_HEIGHT / 2 + 500);
  is(graph.getViewRange().startTime | 0, 0,
    "The selection start boundary is correct (6).");
  is(graph.getViewRange().endTime | 0, 150,
    "The selection end boundary is correct (6).");
  is(graph.getViewRange().verticalOffset | 0, 0,
    "The vertical offset is correct (6).");

  // Drag up slightly vertically and strongly horizontally.

  dragStart(graph, TEST_WIDTH / 2, TEST_HEIGHT / 2);
  is(graph.getViewRange().startTime | 0, 0,
    "The selection start boundary is correct (7).");
  is(graph.getViewRange().endTime | 0, 150,
    "The selection end boundary is correct (7).");
  is(graph.getViewRange().verticalOffset | 0, 0,
    "The vertical offset is correct (7).");

  hover(graph, TEST_WIDTH / 2 + 50, TEST_HEIGHT / 2);
  is(graph.getViewRange().startTime | 0, 0,
    "The selection start boundary is correct (8).");
  is(graph.getViewRange().endTime | 0, 116,
    "The selection end boundary is correct (8).");
  is(graph.getViewRange().verticalOffset | 0, 0,
    "The vertical offset is correct (8).");

  dragStop(graph, TEST_WIDTH / 2 + 500, TEST_HEIGHT / 2 + 100);
  is(graph.getViewRange().startTime | 0, 0,
    "The selection start boundary is correct (9).");
  is(graph.getViewRange().endTime | 0, 0,
    "The selection end boundary is correct (9).");
  is(graph.getViewRange().verticalOffset | 0, 0,
    "The vertical offset is correct (9).");
}

// EventUtils just doesn't work!

function hover(graph, x, y = 1) {
  x /= window.devicePixelRatio;
  y /= window.devicePixelRatio;
  graph._onMouseMove({ testX: x, testY: y });
}

function dragStart(graph, x, y = 1) {
  x /= window.devicePixelRatio;
  y /= window.devicePixelRatio;
  graph._onMouseMove({ testX: x, testY: y });
  graph._onMouseDown({ testX: x, testY: y });
}

function dragStop(graph, x, y = 1) {
  x /= window.devicePixelRatio;
  y /= window.devicePixelRatio;
  graph._onMouseMove({ testX: x, testY: y });
  graph._onMouseUp({ testX: x, testY: y });
}
