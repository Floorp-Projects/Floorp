/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that graph widgets can handle clients getting/setting the
// selection or cursor.

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

  yield testSelection(graph);
  yield testCursor(graph);

  graph.destroy();
  host.destroy();
}

function* testSelection(graph) {
  ok(graph.getSelection().start === null,
    "The graph's selection should initially have a null start value.");
  ok(graph.getSelection().end === null,
    "The graph's selection should initially have a null end value.");
  ok(!graph.hasSelection(),
    "There shouldn't initially be any selection.");

  let selected = graph.once("selecting");
  graph.setSelection({ start: 100, end: 200 });

  yield selected;
  ok(true, "A 'selecting' event has been fired.");

  ok(graph.hasSelection(),
    "There should now be a selection.");
  is(graph.getSelection().start, 100,
    "The graph's selection now has an updated start value.");
  is(graph.getSelection().end, 200,
    "The graph's selection now has an updated end value.");

  let thrown;
  try {
    graph.setSelection({ start: null, end: null });
  } catch(e) {
    thrown = true;
  }
  ok(thrown, "Setting a null selection shouldn't work.");

  ok(graph.hasSelection(),
    "There should still be a selection.");

  let deselected = graph.once("deselecting");
  graph.dropSelection();

  yield deselected;
  ok(true, "A 'deselecting' event has been fired.");

  ok(!graph.hasSelection(),
    "There shouldn't be any selection anymore.");
  ok(graph.getSelection().start === null,
    "The graph's selection now has a null start value.");
  ok(graph.getSelection().end === null,
    "The graph's selection now has a null end value.");
}

function* testCursor(graph) {
  ok(graph.getCursor().x === null,
    "The graph's cursor should initially have a null X value.");
  ok(graph.getCursor().y === null,
    "The graph's cursor should initially have a null Y value.");
  ok(!graph.hasCursor(),
    "There shouldn't initially be any cursor.");

  graph.setCursor({ x: 100, y: 50 });

  ok(graph.hasCursor(),
    "There should now be a cursor.");
  is(graph.getCursor().x, 100,
    "The graph's cursor now has an updated start value.");
  is(graph.getCursor().y, 50,
    "The graph's cursor now has an updated end value.");

  let thrown;
  try {
    graph.setCursor({ x: null, y: null });
  } catch(e) {
    thrown = true;
  }
  ok(thrown, "Setting a null cursor shouldn't work.");

  ok(graph.hasCursor(),
    "There should still be a cursor.");

  graph.dropCursor();

  ok(!graph.hasCursor(),
    "There shouldn't be any cursor anymore.");
  ok(graph.getCursor().x === null,
    "The graph's cursor now has a null start value.");
  ok(graph.getCursor().y === null,
    "The graph's cursor now has a null end value.");
}
