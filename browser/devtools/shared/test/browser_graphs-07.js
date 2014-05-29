/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests if selecting, resizing, moving selections and zooming in/out works.

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
