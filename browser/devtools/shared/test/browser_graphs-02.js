/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that graph widgets can properly add data and regions.

const TEST_DATA = {"112":48,"213":59,"313":60,"413":59,"530":59,"646":58,"747":60,"863":48,"980":37,"1097":30,"1213":29,"1330":23,"1430":10,"1534":17,"1645":20,"1746":22,"1846":39,"1963":26,"2080":27,"2197":35,"2312":47,"2412":53,"2514":60,"2630":37,"2730":36,"2830":37,"2946":36,"3046":40,"3163":47,"3280":41,"3380":35,"3480":27,"3580":39,"3680":42,"3780":49,"3880":55,"3980":60,"4080":60,"4180":60};
const TEST_REGIONS = [{ start: 320, end: 460 }, { start: 780, end: 860 }];
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
  let thrown1;
  try {
    graph.setRegions(TEST_REGIONS);
  } catch (e) {
    thrown1 = true;
  }
  ok(thrown1, "Setting regions before setting data shouldn't work.");

  graph.setData(TEST_DATA);
  graph.setRegions(TEST_REGIONS);

  let thrown2;
  try {
    graph.setRegions(TEST_REGIONS);
  } catch (e) {
    thrown2 = true;
  }
  ok(thrown2, "Setting regions twice shouldn't work.");

  ok(graph.hasData(), "The graph should now have the data source set.");
  ok(graph.hasRegions(), "The graph should now have the regions set.");

  is(graph.dataScaleX,
     graph.width / 4180, // last key in TEST_DATA
    "The data scale on the X axis is correct.");

  is(graph.dataScaleY,
     graph.height / 60 * 0.85, // max value in TEST_DATA * GRAPH_DAMPEN_VALUES
    "The data scale on the Y axis is correct.");

  for (let i = 0; i < TEST_REGIONS.length; i++) {
    let original = TEST_REGIONS[i];
    let normalized = graph._regions[i];

    is(original.start * graph.dataScaleX, normalized.start,
      "The region's start value was properly normalized.");
    is(original.end * graph.dataScaleX, normalized.end,
      "The region's end value was properly normalized.");
  }
}
