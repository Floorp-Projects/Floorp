/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests if a selection is dropped when clicking outside of it.

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

  dragStart(graph, 300);
  dragStop(graph, 500);
  ok(graph.hasSelection(),
    "A selection should be available.");
  is(graph.getSelection().start, 300,
    "The current selection start value is correct.");
  is(graph.getSelection().end, 500,
    "The current selection end value is correct.");

  click(graph, 600);
  ok(!graph.hasSelection(),
    "The selection should be dropped.");
}

// EventUtils just doesn't work!

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
