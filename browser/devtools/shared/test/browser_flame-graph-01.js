/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that flame graph widget works properly.

var {FlameGraph} = require("devtools/shared/widgets/FlameGraph");

add_task(function*() {
  yield promiseTab("about:blank");
  yield performTest();
  gBrowser.removeCurrentTab();
});

function* performTest() {
  let [host, win, doc] = yield createHost();
  doc.body.setAttribute("style", "position: fixed; width: 100%; height: 100%; margin: 0;");

  let graph = new FlameGraph(doc.body);

  let readyEventEmitted;
  graph.once("ready", () => readyEventEmitted = true);

  yield graph.ready();
  ok(readyEventEmitted, "The 'ready' event should have been emitted");

  testGraph(host, graph);

  yield graph.destroy();
  host.destroy();
}

function testGraph(host, graph) {
  ok(graph._container.classList.contains("flame-graph-widget-container"),
    "The correct graph container was created.");
  ok(graph._canvas.classList.contains("flame-graph-widget-canvas"),
    "The correct graph container was created.");

  let bounds = host.frame.getBoundingClientRect();

  is(graph.width, bounds.width * window.devicePixelRatio,
    "The graph has the correct width.");
  is(graph.height, bounds.height * window.devicePixelRatio,
    "The graph has the correct height.");

  ok(graph._selection.start === null,
    "The graph's selection start value is initially null.");
  ok(graph._selection.end === null,
    "The graph's selection end value is initially null.");

  ok(graph._selectionDragger.origin === null,
    "The graph's dragger origin value is initially null.");
  ok(graph._selectionDragger.anchor.start === null,
    "The graph's dragger anchor start value is initially null.");
  ok(graph._selectionDragger.anchor.end === null,
    "The graph's dragger anchor end value is initially null.");
}
