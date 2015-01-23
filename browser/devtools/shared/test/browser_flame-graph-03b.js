/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that selections in the flame graph widget work properly on HiDPI.

let TEST_DATA = [{ color: "#f00", blocks: [{ x: 0, y: 0, width: 50, height: 20, text: "FOO" }, { x: 50, y: 0, width: 100, height: 20, text: "BAR" }] }, { color: "#00f", blocks: [{ x: 0, y: 30, width: 30, height: 20, text: "BAZ" }] }];
let TEST_BOUNDS = { startTime: 0, endTime: 150 };
let TEST_WIDTH = 200;
let TEST_HEIGHT = 100;
let TEST_DPI_DENSITIY = 2;

let {FlameGraph} = Cu.import("resource:///modules/devtools/FlameGraph.jsm", {});
let {DOMHelpers} = Cu.import("resource:///modules/devtools/DOMHelpers.jsm", {});
let {Promise} = devtools.require("resource://gre/modules/Promise.jsm");
let {Hosts} = devtools.require("devtools/framework/toolbox-hosts");

let test = Task.async(function*() {
  yield promiseTab("about:blank");
  yield performTest();
  gBrowser.removeCurrentTab();
  finish();
});

function* performTest() {
  let [host, win, doc] = yield createHost();
  doc.body.setAttribute("style", "position: fixed; width: 100%; height: 100%; margin: 0;");

  let graph = new FlameGraph(doc.body, TEST_DPI_DENSITIY);
  graph.fixedWidth = TEST_WIDTH;
  graph.fixedHeight = TEST_HEIGHT;

  yield graph.ready();

  testGraph(graph);

  graph.destroy();
  host.destroy();
}

function testGraph(graph) {
  graph.setData({ data: TEST_DATA, bounds: TEST_BOUNDS });

  is(graph.getViewRange().startTime, 0,
    "The selection start boundary is correct on HiDPI (1).");
  is(graph.getViewRange().endTime, 150,
    "The selection end boundary is correct on HiDPI (1).");

  is(graph.getOuterBounds().startTime, 0,
    "The bounds start boundary is correct on HiDPI (1).");
  is(graph.getOuterBounds().endTime, 150,
    "The bounds end boundary is correct on HiDPI (1).");

  scroll(graph, 10000, HORIZONTAL_AXIS, 1);

  is(graph.getViewRange().startTime, 140,
    "The selection start boundary is correct on HiDPI (2).");
  is(graph.getViewRange().endTime, 150,
    "The selection end boundary is correct on HiDPI (2).");

  is(graph.getOuterBounds().startTime, 0,
    "The bounds start boundary is correct on HiDPI (2).");
  is(graph.getOuterBounds().endTime, 150,
    "The bounds end boundary is correct on HiDPI (2).");
}

// EventUtils just doesn't work!

let HORIZONTAL_AXIS = 1;
let VERTICAL_AXIS = 2;

function scroll(graph, wheel, axis, x, y = 1) {
  x /= window.devicePixelRatio;
  y /= window.devicePixelRatio;
  graph._onMouseMove({ clientX: x, clientY: y });
  graph._onMouseWheel({ clientX: x, clientY: y, axis, detail: wheel, axis,
    HORIZONTAL_AXIS,
    VERTICAL_AXIS
  });
}
