/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that graph widgets can correctly compare selections and cursors.

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
  ok(!graph.hasSelection(),
    "There shouldn't initially be any selection.");
  is(graph.getSelectionWidth(), 0,
    "The selection width should be 0 when there's no selection.");

  graph.setSelection({ start: 100, end: 200 });

  ok(graph.hasSelection(),
    "There should now be a selection.");
  is(graph.getSelectionWidth(), 100,
    "The selection width should now be 100.");

  ok(graph.isSelectionDifferent({ start: 100, end: 201 }),
    "The selection was correctly reported to be different (1).");
  ok(graph.isSelectionDifferent({ start: 101, end: 200 }),
    "The selection was correctly reported to be different (2).");
  ok(graph.isSelectionDifferent({ start: null, end: null }),
    "The selection was correctly reported to be different (3).");
  ok(graph.isSelectionDifferent(null),
    "The selection was correctly reported to be different (4).");

  ok(!graph.isSelectionDifferent({ start: 100, end: 200 }),
    "The selection was incorrectly reported to be different (1).");
  ok(!graph.isSelectionDifferent(graph.getSelection()),
    "The selection was incorrectly reported to be different (2).");

  graph.setCursor({ x: 100, y: 50 });

  ok(graph.isCursorDifferent({ x: 100, y: 51 }),
    "The cursor was correctly reported to be different (1).");
  ok(graph.isCursorDifferent({ x: 101, y: 50 }),
    "The cursor was correctly reported to be different (2).");
  ok(graph.isCursorDifferent({ x: null, y: null }),
    "The cursor was correctly reported to be different (3).");
  ok(graph.isCursorDifferent(null),
    "The cursor was correctly reported to be different (4).");

  ok(!graph.isCursorDifferent({ x: 100, y: 50 }),
    "The cursor was incorrectly reported to be different (1).");
  ok(!graph.isCursorDifferent(graph.getCursor()),
    "The cursor was incorrectly reported to be different (2).");
}
