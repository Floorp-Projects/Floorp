/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that graphs properly handle resizing.

const TEST_DATA = [{ delta: 112, value: 48 }, { delta: 213, value: 59 }, { delta: 313, value: 60 }, { delta: 413, value: 59 }, { delta: 530, value: 59 }, { delta: 646, value: 58 }, { delta: 747, value: 60 }, { delta: 863, value: 48 }, { delta: 980, value: 37 }, { delta: 1097, value: 30 }, { delta: 1213, value: 29 }, { delta: 1330, value: 23 }, { delta: 1430, value: 10 }, { delta: 1534, value: 17 }, { delta: 1645, value: 20 }, { delta: 1746, value: 22 }, { delta: 1846, value: 39 }, { delta: 1963, value: 26 }, { delta: 2080, value: 27 }, { delta: 2197, value: 35 }, { delta: 2312, value: 47 }, { delta: 2412, value: 53 }, { delta: 2514, value: 60 }, { delta: 2630, value: 37 }, { delta: 2730, value: 36 }, { delta: 2830, value: 37 }, { delta: 2946, value: 36 }, { delta: 3046, value: 40 }, { delta: 3163, value: 47 }, { delta: 3280, value: 41 }, { delta: 3380, value: 35 }, { delta: 3480, value: 27 }, { delta: 3580, value: 39 }, { delta: 3680, value: 42 }, { delta: 3780, value: 49 }, { delta: 3880, value: 55 }, { delta: 3980, value: 60 }, { delta: 4080, value: 60 }, { delta: 4180, value: 60 }];
var LineGraphWidget = require("devtools/client/shared/widgets/LineGraphWidget");

add_task(function*() {
  yield promiseTab("about:blank");
  yield performTest();
  gBrowser.removeCurrentTab();
});

function* performTest() {
  let [host, win, doc] = yield createHost("window");
  doc.body.setAttribute("style", "position: fixed; width: 100%; height: 100%; margin: 0;");

  let graph = new LineGraphWidget(doc.body, "fps");
  yield graph.once("ready");

  let refreshCount = 0;
  graph.on("refresh", () => refreshCount++);

  yield testGraph(host, graph);

  is(refreshCount, 2, "The graph should've been refreshed 2 times.");

  yield graph.destroy();
  host.destroy();
}

function* testGraph(host, graph) {
  graph.setData(TEST_DATA);
  let initialBounds = host.frame.getBoundingClientRect();

  host._window.resizeBy(-100, -100);
  yield graph.once("refresh");
  let newBounds = host.frame.getBoundingClientRect();

  is(initialBounds.width - newBounds.width, 100,
    "The window was properly resized (1).");
  is(initialBounds.height - newBounds.height, 100,
    "The window was properly resized (2).");

  is(graph.width, newBounds.width * window.devicePixelRatio,
    "The graph has the correct width (1).");
  is(graph.height, newBounds.height * window.devicePixelRatio,
    "The graph has the correct height (1).");

  info("Making a selection.");

  dragStart(graph, 300);
  ok(graph.hasSelectionInProgress(),
    "The selection should start (1).");
  is(graph.getSelection().start, 300,
    "The current selection start value is correct (1).");
  is(graph.getSelection().end, 300,
    "The current selection end value is correct (1).");

  hover(graph, 400);
  ok(graph.hasSelectionInProgress(),
    "The selection should still be in progress (2).");
  is(graph.getSelection().start, 300,
    "The current selection start value is correct (2).");
  is(graph.getSelection().end, 400,
    "The current selection end value is correct (2).");

  dragStop(graph, 500);
  ok(!graph.hasSelectionInProgress(),
    "The selection should have stopped (3).");
  is(graph.getSelection().start, 300,
    "The current selection start value is correct (3).");
  is(graph.getSelection().end, 500,
    "The current selection end value is correct (3).");

  host._window.resizeBy(100, 100);
  yield graph.once("refresh");
  let newerBounds = host.frame.getBoundingClientRect();

  is(initialBounds.width - newerBounds.width, 0,
    "The window was properly resized (3).");
  is(initialBounds.height - newerBounds.height, 0,
    "The window was properly resized (4).");

  is(graph.width, newerBounds.width * window.devicePixelRatio,
    "The graph has the correct width (2).");
  is(graph.height, newerBounds.height * window.devicePixelRatio,
    "The graph has the correct height (2).");

  info("Making a new selection.");

  dragStart(graph, 200);
  ok(graph.hasSelectionInProgress(),
    "The selection should start (4).");
  is(graph.getSelection().start, 200,
    "The current selection start value is correct (4).");
  is(graph.getSelection().end, 200,
    "The current selection end value is correct (4).");

  hover(graph, 300);
  ok(graph.hasSelectionInProgress(),
    "The selection should still be in progress (5).");
  is(graph.getSelection().start, 200,
    "The current selection start value is correct (5).");
  is(graph.getSelection().end, 300,
    "The current selection end value is correct (5).");

  dragStop(graph, 400);
  ok(!graph.hasSelectionInProgress(),
    "The selection should have stopped (6).");
  is(graph.getSelection().start, 200,
    "The current selection start value is correct (6).");
  is(graph.getSelection().end, 400,
    "The current selection end value is correct (6).");
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
