/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that line graph properly handles resizing.

const TEST_DATA = {"112":48,"213":59,"313":60,"413":59,"530":59,"646":58,"747":60,"863":48,"980":37,"1097":30,"1213":29,"1330":23,"1430":10,"1534":17,"1645":20,"1746":22,"1846":39,"1963":26,"2080":27,"2197":35,"2312":47,"2412":53,"2514":60,"2630":37,"2730":36,"2830":37,"2946":36,"3046":40,"3163":47,"3280":41,"3380":35,"3480":27,"3580":39,"3680":42,"3780":49,"3880":55,"3980":60,"4080":60,"4180":60};
let {LineGraphWidget} = Cu.import("resource:///modules/devtools/Graphs.jsm", {});
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
  let [host, win, doc] = yield createHost("window");
  doc.body.setAttribute("style", "position: fixed; width: 100%; height: 100%; margin: 0;");

  let graph = new LineGraphWidget(doc.body, "fps");
  yield graph.once("ready");

  let refreshCount = 0;
  graph.on("refresh", () => refreshCount++);

  yield testGraph(host, graph);

  is(refreshCount, 2, "The graph should've been refreshed 2 times.");

  graph.destroy();
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
  graph._onMouseMove({ clientX: x, clientY: y });
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
