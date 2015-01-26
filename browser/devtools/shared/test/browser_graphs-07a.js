/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests if selecting, resizing, moving selections and zooming in/out works.

const TEST_DATA = [{ delta: 112, value: 48 }, { delta: 213, value: 59 }, { delta: 313, value: 60 }, { delta: 413, value: 59 }, { delta: 530, value: 59 }, { delta: 646, value: 58 }, { delta: 747, value: 60 }, { delta: 863, value: 48 }, { delta: 980, value: 37 }, { delta: 1097, value: 30 }, { delta: 1213, value: 29 }, { delta: 1330, value: 23 }, { delta: 1430, value: 10 }, { delta: 1534, value: 17 }, { delta: 1645, value: 20 }, { delta: 1746, value: 22 }, { delta: 1846, value: 39 }, { delta: 1963, value: 26 }, { delta: 2080, value: 27 }, { delta: 2197, value: 35 }, { delta: 2312, value: 47 }, { delta: 2412, value: 53 }, { delta: 2514, value: 60 }, { delta: 2630, value: 37 }, { delta: 2730, value: 36 }, { delta: 2830, value: 37 }, { delta: 2946, value: 36 }, { delta: 3046, value: 40 }, { delta: 3163, value: 47 }, { delta: 3280, value: 41 }, { delta: 3380, value: 35 }, { delta: 3480, value: 27 }, { delta: 3580, value: 39 }, { delta: 3680, value: 42 }, { delta: 3780, value: 49 }, { delta: 3880, value: 55 }, { delta: 3980, value: 60 }, { delta: 4080, value: 60 }, { delta: 4180, value: 60 }];
let {LineGraphWidget} = Cu.import("resource:///modules/devtools/Graphs.jsm", {});
let {Promise} = devtools.require("resource://gre/modules/Promise.jsm");

add_task(function*() {
  yield promiseTab("about:blank");
  yield performTest();
  gBrowser.removeCurrentTab();
});

function* performTest() {
  let [host, win, doc] = yield createHost();
  let graph = new LineGraphWidget(doc.body, "fps");
  yield graph.once("ready");

  testGraph(graph);

  graph.destroy();
  host.destroy();
}

function testGraph(graph) {
  graph.setData(TEST_DATA);

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

  info("Resizing by dragging the end handlebar.");

  dragStart(graph, 400);
  is(graph.getSelection().start, 200,
    "The current selection start value is correct (7).");
  is(graph.getSelection().end, 400,
    "The current selection end value is correct (7).");

  dragStop(graph, 600);
  is(graph.getSelection().start, 200,
    "The current selection start value is correct (8).");
  is(graph.getSelection().end, 600,
    "The current selection end value is correct (8).");

  info("Resizing by dragging the start handlebar.");

  dragStart(graph, 200);
  is(graph.getSelection().start, 200,
    "The current selection start value is correct (9).");
  is(graph.getSelection().end, 600,
    "The current selection end value is correct (9).");

  dragStop(graph, 100);
  is(graph.getSelection().start, 100,
    "The current selection start value is correct (10).");
  is(graph.getSelection().end, 600,
    "The current selection end value is correct (10).");

  info("Moving by dragging the selection.");

  dragStart(graph, 300);
  hover(graph, 400);
  is(graph.getSelection().start, 200,
    "The current selection start value is correct (11).");
  is(graph.getSelection().end, 700,
    "The current selection end value is correct (11).");

  dragStop(graph, 500);
  is(graph.getSelection().start, 300,
    "The current selection start value is correct (12).");
  is(graph.getSelection().end, 800,
    "The current selection end value is correct (12).");

  info("Zooming in by scrolling inside the selection.");

  scroll(graph, -1000, 600);
  is(graph.getSelection().start, 525,
    "The current selection start value is correct (13).");
  is(graph.getSelection().end, 650,
    "The current selection end value is correct (13).");

  info("Zooming out by scrolling inside the selection.");

  scroll(graph, 1000, 600);
  is(graph.getSelection().start, 468.75,
    "The current selection start value is correct (14).");
  is(graph.getSelection().end, 687.5,
    "The current selection end value is correct (14).");

  info("Sliding left by scrolling outside the selection.");

  scroll(graph, 100, 900);
  is(graph.getSelection().start, 458.75,
    "The current selection start value is correct (15).");
  is(graph.getSelection().end, 677.5,
    "The current selection end value is correct (15).");

  info("Sliding right by scrolling outside the selection.");

  scroll(graph, -100, 900);
  is(graph.getSelection().start, 468.75,
    "The current selection start value is correct (16).");
  is(graph.getSelection().end, 687.5,
    "The current selection end value is correct (16).");

  info("Zooming out a lot.");

  scroll(graph, Number.MAX_SAFE_INTEGER, 500);
  is(graph.getSelection().start, 1,
    "The current selection start value is correct (17).");
  is(graph.getSelection().end, graph.width - 1,
    "The current selection end value is correct (17).");
}

// EventUtils just doesn't work!

function hover(graph, x, y = 1) {
  x /= window.devicePixelRatio;
  y /= window.devicePixelRatio;
  graph._onMouseMove({ clientX: x, clientY: y });
}

function click(graph, x, y = 1) {
  x /= window.devicePixelRatio;
  y /= window.devicePixelRatio;
  graph._onMouseMove({ clientX: x, clientY: y });
  graph._onMouseDown({ clientX: x, clientY: y });
  graph._onMouseUp({ clientX: x, clientY: y });
}

function dragStart(graph, x, y = 1) {
  x /= window.devicePixelRatio;
  y /= window.devicePixelRatio;
  graph._onMouseMove({ clientX: x, clientY: y });
  graph._onMouseDown({ clientX: x, clientY: y });
}

function dragStop(graph, x, y = 1) {
  x /= window.devicePixelRatio;
  y /= window.devicePixelRatio;
  graph._onMouseMove({ clientX: x, clientY: y });
  graph._onMouseUp({ clientX: x, clientY: y });
}

function scroll(graph, wheel, x, y = 1) {
  x /= window.devicePixelRatio;
  y /= window.devicePixelRatio;
  graph._onMouseMove({ clientX: x, clientY: y });
  graph._onMouseWheel({ clientX: x, clientY: y, detail: wheel });
}
