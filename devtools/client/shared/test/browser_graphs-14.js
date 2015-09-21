/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that graph widgets correctly emit mouse input events.

const TEST_DATA = [{ delta: 112, value: 48 }, { delta: 213, value: 59 }, { delta: 313, value: 60 }, { delta: 413, value: 59 }, { delta: 530, value: 59 }, { delta: 646, value: 58 }, { delta: 747, value: 60 }, { delta: 863, value: 48 }, { delta: 980, value: 37 }, { delta: 1097, value: 30 }, { delta: 1213, value: 29 }, { delta: 1330, value: 23 }, { delta: 1430, value: 10 }, { delta: 1534, value: 17 }, { delta: 1645, value: 20 }, { delta: 1746, value: 22 }, { delta: 1846, value: 39 }, { delta: 1963, value: 26 }, { delta: 2080, value: 27 }, { delta: 2197, value: 35 }, { delta: 2312, value: 47 }, { delta: 2412, value: 53 }, { delta: 2514, value: 60 }, { delta: 2630, value: 37 }, { delta: 2730, value: 36 }, { delta: 2830, value: 37 }, { delta: 2946, value: 36 }, { delta: 3046, value: 40 }, { delta: 3163, value: 47 }, { delta: 3280, value: 41 }, { delta: 3380, value: 35 }, { delta: 3480, value: 27 }, { delta: 3580, value: 39 }, { delta: 3680, value: 42 }, { delta: 3780, value: 49 }, { delta: 3880, value: 55 }, { delta: 3980, value: 60 }, { delta: 4080, value: 60 }, { delta: 4180, value: 60 }];
var LineGraphWidget = require("devtools/client/shared/widgets/LineGraphWidget");

add_task(function*() {
  yield promiseTab("about:blank");
  yield performTest();
  gBrowser.removeCurrentTab();
});

function* performTest() {
  let [host, win, doc] = yield createHost();
  let graph = new LineGraphWidget(doc.body, "fps");

  yield testGraph(graph);

  yield graph.destroy();
  host.destroy();
}

function* testGraph(graph) {
  let mouseDownEvents = 0;
  let mouseUpEvents = 0;
  let scrollEvents = 0;
  graph.on("mousedown", () => mouseDownEvents++);
  graph.on("mouseup", () => mouseUpEvents++);
  graph.on("scroll", () => scrollEvents++);

  yield graph.setDataWhenReady(TEST_DATA);

  info("Making a selection.");

  dragStart(graph, 300);
  dragStop(graph, 500);
  is(graph.getSelection().start, 300,
    "The current selection start value is correct (1).");
  is(graph.getSelection().end, 500,
    "The current selection end value is correct (1).");

  is(mouseDownEvents, 1,
    "One mousedown event should have been fired.");
  is(mouseUpEvents, 1,
    "One mouseup event should have been fired.");
  is(scrollEvents, 0,
    "No scroll event should have been fired.");

  info("Zooming in by scrolling inside the selection.");

  scroll(graph, -1000, 400);
  is(graph.getSelection().start, 375,
    "The current selection start value is correct (2).");
  is(graph.getSelection().end, 425,
    "The current selection end value is correct (2).");

  is(mouseDownEvents, 1,
    "No more mousedown events should have been fired.");
  is(mouseUpEvents, 1,
    "No more mouseup events should have been fired.");
  is(scrollEvents, 1,
    "One scroll event should have been fired.");
}

// EventUtils just doesn't work!

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

function scroll(graph, wheel, x, y = 1) {
  x /= window.devicePixelRatio;
  y /= window.devicePixelRatio;
  graph._onMouseMove({ testX: x, testY: y });
  graph._onMouseWheel({ testX: x, testY: y, detail: wheel });
}
