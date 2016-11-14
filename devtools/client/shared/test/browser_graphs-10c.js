
"use strict";

// Tests that graphs properly handle resizing.

const TEST_DATA = [
  { delta: 112, value: 48 }, { delta: 213, value: 59 },
  { delta: 313, value: 60 }, { delta: 413, value: 59 },
  { delta: 530, value: 59 }, { delta: 646, value: 58 },
  { delta: 747, value: 60 }, { delta: 863, value: 48 },
  { delta: 980, value: 37 }, { delta: 1097, value: 30 },
  { delta: 1213, value: 29 }, { delta: 1330, value: 23 },
  { delta: 1430, value: 10 }, { delta: 1534, value: 17 },
  { delta: 1645, value: 20 }, { delta: 1746, value: 22 },
  { delta: 1846, value: 39 }, { delta: 1963, value: 26 },
  { delta: 2080, value: 27 }, { delta: 2197, value: 35 },
  { delta: 2312, value: 47 }, { delta: 2412, value: 53 },
  { delta: 2514, value: 60 }, { delta: 2630, value: 37 },
  { delta: 2730, value: 36 }, { delta: 2830, value: 37 },
  { delta: 2946, value: 36 }, { delta: 3046, value: 40 },
  { delta: 3163, value: 47 }, { delta: 3280, value: 41 },
  { delta: 3380, value: 35 }, { delta: 3480, value: 27 },
  { delta: 3580, value: 39 }, { delta: 3680, value: 42 },
  { delta: 3780, value: 49 }, { delta: 3880, value: 55 },
  { delta: 3980, value: 60 }, { delta: 4080, value: 60 },
  { delta: 4180, value: 60 }
];
const LineGraphWidget = require("devtools/client/shared/widgets/LineGraphWidget");

add_task(function* () {
  yield addTab("about:blank");
  yield performTest();
  gBrowser.removeCurrentTab();
});

function* performTest() {
  let [host,, doc] = yield createHost("window");
  doc.body.setAttribute("style",
                        "position: fixed; width: 100%; height: 100%; margin: 0;");

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

  host._window.resizeTo(500, 500);
  yield graph.once("refresh");
  let oldBounds = host.frame.getBoundingClientRect();

  is(graph._width, oldBounds.width * window.devicePixelRatio,
    "The window was properly resized (1).");
  is(graph._height, oldBounds.height * window.devicePixelRatio,
    "The window was properly resized (1).");

  dragStart(graph, 100);
  dragStop(graph, 400);

  is(graph.getSelection().start, 100,
    "The current selection start value is correct (1).");
  is(graph.getSelection().end, 400,
    "The current selection end value is correct (1).");

  info("Making sure the selection updates when the window is resized");

  host._window.resizeTo(250, 250);
  yield graph.once("refresh");
  let newBounds = host.frame.getBoundingClientRect();

  is(graph._width, newBounds.width * window.devicePixelRatio,
    "The window was properly resized (2).");
  is(graph._height, newBounds.height * window.devicePixelRatio,
    "The window was properly resized (2).");

  let ratio = oldBounds.width / newBounds.width;
  info("The window resize ratio is: " + ratio);

  is(graph.getSelection().start, Math.round(100 / ratio),
    "The current selection start value is correct (2).");
  is(graph.getSelection().end, Math.round(400 / ratio),
    "The current selection end value is correct (2).");
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
