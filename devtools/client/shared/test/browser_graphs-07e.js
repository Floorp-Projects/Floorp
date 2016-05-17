/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that selections are drawn onto the canvas.

const TEST_DATA = [{ delta: 112, value: 48 }, { delta: 213, value: 59 }, { delta: 313, value: 60 }, { delta: 413, value: 59 }, { delta: 530, value: 59 }, { delta: 646, value: 58 }, { delta: 747, value: 60 }, { delta: 863, value: 48 }, { delta: 980, value: 37 }, { delta: 1097, value: 30 }, { delta: 1213, value: 29 }, { delta: 1330, value: 23 }, { delta: 1430, value: 10 }, { delta: 1534, value: 17 }, { delta: 1645, value: 20 }, { delta: 1746, value: 22 }, { delta: 1846, value: 39 }, { delta: 1963, value: 26 }, { delta: 2080, value: 27 }, { delta: 2197, value: 35 }, { delta: 2312, value: 47 }, { delta: 2412, value: 53 }, { delta: 2514, value: 60 }, { delta: 2630, value: 37 }, { delta: 2730, value: 36 }, { delta: 2830, value: 37 }, { delta: 2946, value: 36 }, { delta: 3046, value: 40 }, { delta: 3163, value: 47 }, { delta: 3280, value: 41 }, { delta: 3380, value: 35 }, { delta: 3480, value: 27 }, { delta: 3580, value: 39 }, { delta: 3680, value: 42 }, { delta: 3780, value: 49 }, { delta: 3880, value: 55 }, { delta: 3980, value: 60 }, { delta: 4080, value: 60 }, { delta: 4180, value: 60 }];
var LineGraphWidget = require("devtools/client/shared/widgets/LineGraphWidget");
var CURRENT_ZOOM = 1;

add_task(function* () {
  yield addTab("about:blank");
  yield performTest();
  gBrowser.removeCurrentTab();
});

function* performTest() {
  let [host, win, doc] = yield createHost();
  let graph = new LineGraphWidget(doc.body, "fps");
  yield graph.once("ready");
  graph.setData(TEST_DATA);

  info("Testing with normal zoom.");
  testGraph(graph);

  info("Testing while zoomed out.");
  setZoom(host.frame, .5);
  testGraph(graph);

  info("Testing while zoomed in.");
  setZoom(host.frame, 2);
  testGraph(graph);

  yield graph.destroy();
  host.destroy();
}

function testGraph(graph) {
  graph.dropSelection();

  info("Making a selection.");

  dragStart(graph, 100);
  ok(graph.hasSelectionInProgress(),
    "The selection should start (1).");
  is(graph.getSelection().start, 100,
    "The current selection start value is correct (1).");
  is(graph.getSelection().end, 100,
    "The current selection end value is correct (1).");

  hover(graph, 200);
  ok(graph.hasSelectionInProgress(),
    "The selection should still be in progress (2).");
  is(graph.getSelection().start, 100,
    "The current selection start value is correct (2).");
  is(graph.getSelection().end, 200,
    "The current selection end value is correct (2).");

  dragStop(graph, 300);
  ok(!graph.hasSelectionInProgress(),
    "The selection should have stopped (3).");
  is(graph.getSelection().start, 100,
    "The current selection start value is correct (3).");
  is(graph.getSelection().end, 300,
    "The current selection end value is correct (3).");
}

function setZoom(frame, zoomValue) {
  let contViewer = frame.docShell.contentViewer;
  CURRENT_ZOOM = contViewer.fullZoom = zoomValue;
}

// EventUtils just doesn't work!

function dispatchEvent(graph, x, y, fn) {
  x *= CURRENT_ZOOM;
  y *= CURRENT_ZOOM;
  x /= window.devicePixelRatio;
  y /= window.devicePixelRatio;
  let quad = graph._canvas.getBoxQuads({
    relativeTo: window.document
  })[0];

  let screenX = (window.screenX + quad.p1.x + x);
  let screenY = (window.screenY + quad.p1.y + y);

  fn({
    screenX: screenX,
    screenY: screenY,
  });
}

function hover(graph, x, y = 1) {
  dispatchEvent(graph, x, y, graph._onMouseMove);
}

function dragStart(graph, x, y = 1) {
  dispatchEvent(graph, x, y, graph._onMouseMove);
  dispatchEvent(graph, x, y, graph._onMouseDown);
}

function dragStop(graph, x, y = 1) {
  dispatchEvent(graph, x, y, graph._onMouseMove);
  dispatchEvent(graph, x, y, graph._onMouseUp);
}
