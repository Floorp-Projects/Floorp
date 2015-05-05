/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that canvas graphs can have their selection linked.

let {LineGraphWidget, BarGraphWidget, CanvasGraphUtils} = Cu.import("resource:///modules/devtools/Graphs.jsm", {});
let {Promise} = devtools.require("resource://gre/modules/Promise.jsm");

add_task(function*() {
  yield promiseTab("about:blank");
  yield performTest();
  gBrowser.removeCurrentTab();
});

function* performTest() {
  let [host, win, doc] = yield createHost();
  doc.body.setAttribute("style", "position: fixed; width: 100%; height: 100%; margin: 0;");

  let first = document.createElement("div");
  first.setAttribute("style", "display: inline-block; width: 100%; height: 50%;");
  doc.body.appendChild(first);

  let second = document.createElement("div");
  second.setAttribute("style", "display: inline-block; width: 100%; height: 50%;");
  doc.body.appendChild(second);

  let graph1 = new LineGraphWidget(first, "js");
  let graph2 = new BarGraphWidget(second);

  CanvasGraphUtils.linkAnimation(graph1, graph2);
  CanvasGraphUtils.linkSelection(graph1, graph2);

  yield graph1.ready();
  yield graph2.ready();

  testGraphs(graph1, graph2);

  yield graph1.destroy();
  yield graph2.destroy();
  host.destroy();
}

function testGraphs(graph1, graph2) {
  info("Making a selection in the first graph.");

  dragStart(graph1, 300);
  ok(graph1.hasSelectionInProgress(),
    "The selection should start (1.1).");
  ok(!graph2.hasSelectionInProgress(),
    "The selection should not start yet in the second graph (1.2).");
  is(graph1.getSelection().start, 300,
    "The current selection start value is correct (1.1).");
  is(graph2.getSelection().start, 300,
    "The current selection start value is correct (1.2).");
  is(graph1.getSelection().end, 300,
    "The current selection end value is correct (1.1).");
  is(graph2.getSelection().end, 300,
    "The current selection end value is correct (1.2).");

  hover(graph1, 400);
  ok(graph1.hasSelectionInProgress(),
    "The selection should still be in progress (2.1).");
  ok(!graph2.hasSelectionInProgress(),
    "The selection should not be in progress in the second graph (2.2).");
  is(graph1.getSelection().start, 300,
    "The current selection start value is correct (2.1).");
  is(graph2.getSelection().start, 300,
    "The current selection start value is correct (2.2).");
  is(graph1.getSelection().end, 400,
    "The current selection end value is correct (2.1).");
  is(graph2.getSelection().end, 400,
    "The current selection end value is correct (2.2).");

  dragStop(graph1, 500);
  ok(!graph1.hasSelectionInProgress(),
    "The selection should have stopped (3.1).");
  ok(!graph2.hasSelectionInProgress(),
    "The selection should have stopped (3.2).");
  is(graph1.getSelection().start, 300,
    "The current selection start value is correct (3.1).");
  is(graph2.getSelection().start, 300,
    "The current selection start value is correct (3.2).");
  is(graph1.getSelection().end, 500,
    "The current selection end value is correct (3.1).");
  is(graph2.getSelection().end, 500,
    "The current selection end value is correct (3.2).");

  info("Making a new selection in the second graph.");

  dragStart(graph2, 200);
  ok(!graph1.hasSelectionInProgress(),
    "The selection should not start yet in the first graph (4.1).");
  ok(graph2.hasSelectionInProgress(),
    "The selection should start (4.2).");
  is(graph1.getSelection().start, 200,
    "The current selection start value is correct (4.1).");
  is(graph2.getSelection().start, 200,
    "The current selection start value is correct (4.2).");
  is(graph1.getSelection().end, 200,
    "The current selection end value is correct (4.1).");
  is(graph2.getSelection().end, 200,
    "The current selection end value is correct (4.2).");

  hover(graph2, 300);
  ok(!graph1.hasSelectionInProgress(),
    "The selection should not be in progress in the first graph (2.2).");
  ok(graph2.hasSelectionInProgress(),
    "The selection should still be in progress (5.2).");
  is(graph1.getSelection().start, 200,
    "The current selection start value is correct (5.1).");
  is(graph2.getSelection().start, 200,
    "The current selection start value is correct (5.2).");
  is(graph1.getSelection().end, 300,
    "The current selection end value is correct (5.1).");
  is(graph2.getSelection().end, 300,
    "The current selection end value is correct (5.2).");

  dragStop(graph2, 400);
  ok(!graph1.hasSelectionInProgress(),
    "The selection should have stopped (6.1).");
  ok(!graph2.hasSelectionInProgress(),
    "The selection should have stopped (6.2).");
  is(graph1.getSelection().start, 200,
    "The current selection start value is correct (6.1).");
  is(graph2.getSelection().start, 200,
    "The current selection start value is correct (6.2).");
  is(graph1.getSelection().end, 400,
    "The current selection end value is correct (6.1).");
  is(graph2.getSelection().end, 400,
    "The current selection end value is correct (6.2).");
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
