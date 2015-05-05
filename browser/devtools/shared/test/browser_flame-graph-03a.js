/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that selections in the flame graph widget work properly.

let TEST_DATA = [{ color: "#f00", blocks: [{ x: 0, y: 0, width: 50, height: 20, text: "FOO" }, { x: 50, y: 0, width: 100, height: 20, text: "BAR" }] }, { color: "#00f", blocks: [{ x: 0, y: 30, width: 30, height: 20, text: "BAZ" }] }];
let TEST_BOUNDS = { startTime: 0, endTime: 150 };
let TEST_WIDTH = 200;
let TEST_HEIGHT = 100;

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

  let graph = new FlameGraph(doc.body, 1);
  graph.fixedWidth = TEST_WIDTH;
  graph.fixedHeight = TEST_HEIGHT;
  graph.horizontalPanThreshold = 0;
  graph.verticalPanThreshold = 0;

  yield graph.ready();

  testGraph(graph);

  yield graph.destroy();
  host.destroy();
}

function testGraph(graph) {
  graph.setData({ data: TEST_DATA, bounds: TEST_BOUNDS });

  is(graph.getViewRange().startTime, 0,
    "The selection start boundary is correct (1).");
  is(graph.getViewRange().endTime, 150,
    "The selection end boundary is correct (1).");

  scroll(graph, 200, HORIZONTAL_AXIS, 10);
  is(graph.getViewRange().startTime | 0, 75,
    "The selection start boundary is correct (2).");
  is(graph.getViewRange().endTime | 0, 150,
    "The selection end boundary is correct (2).");

  scroll(graph, -200, HORIZONTAL_AXIS, 10);
  is(graph.getViewRange().startTime | 0, 37,
    "The selection start boundary is correct (3).");
  is(graph.getViewRange().endTime | 0, 112,
    "The selection end boundary is correct (3).");

  scroll(graph, 200, VERTICAL_AXIS, TEST_WIDTH / 2);
  is(graph.getViewRange().startTime | 0, 34,
    "The selection start boundary is correct (4).");
  is(graph.getViewRange().endTime | 0, 115,
    "The selection end boundary is correct (4).");

  scroll(graph, -200, VERTICAL_AXIS, TEST_WIDTH / 2);
  is(graph.getViewRange().startTime | 0, 37,
    "The selection start boundary is correct (5).");
  is(graph.getViewRange().endTime | 0, 112,
    "The selection end boundary is correct (5).");

  dragStart(graph, TEST_WIDTH / 2);
  is(graph.getViewRange().startTime | 0, 37,
    "The selection start boundary is correct (6).");
  is(graph.getViewRange().endTime | 0, 112,
    "The selection end boundary is correct (6).");

  hover(graph, TEST_WIDTH / 2 - 10);
  is(graph.getViewRange().startTime | 0, 41,
    "The selection start boundary is correct (7).");
  is(graph.getViewRange().endTime | 0, 116,
    "The selection end boundary is correct (7).");

  dragStop(graph, 10);
  is(graph.getViewRange().startTime | 0, 71,
    "The selection start boundary is correct (8).");
  is(graph.getViewRange().endTime | 0, 145,
    "The selection end boundary is correct (8).");
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

let HORIZONTAL_AXIS = 1;
let VERTICAL_AXIS = 2;

function scroll(graph, wheel, axis, x, y = 1) {
  x /= window.devicePixelRatio;
  y /= window.devicePixelRatio;
  graph._onMouseMove({ testX: x, testY: y });
  graph._onMouseWheel({ testX: x, testY: y, axis, detail: wheel, axis,
    HORIZONTAL_AXIS,
    VERTICAL_AXIS
  });
}
