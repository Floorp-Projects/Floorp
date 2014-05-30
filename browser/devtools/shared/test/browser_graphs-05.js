/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that graph widgets can correctly determine which regions are hovered.

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
  ok(!graph.getHoveredRegion(),
    "There should be no hovered region yet because there's no regions.");

  ok(!graph._isHoveringStartBoundary(),
    "The graph start boundary should not be hovered.");
  ok(!graph._isHoveringEndBoundary(),
    "The graph end boundary should not be hovered.");
  ok(!graph._isHoveringSelectionContents(),
    "The graph contents should not be hovered.");
  ok(!graph._isHoveringSelectionContentsOrBoundaries(),
    "The graph contents or boundaries should not be hovered.");

  graph.setData(TEST_DATA);
  graph.setRegions(TEST_REGIONS);

  ok(!graph.getHoveredRegion(),
    "There should be no hovered region yet because there's no cursor.");

  graph.setCursor({ x: TEST_REGIONS[0].start * graph.dataScaleX - 1, y: 0 });
  ok(!graph.getHoveredRegion(),
    "There shouldn't be any hovered region yet.");

  graph.setCursor({ x: TEST_REGIONS[0].start * graph.dataScaleX + 1, y: 0 });
  ok(graph.getHoveredRegion(),
    "There should be a hovered region now.");
  is(graph.getHoveredRegion().start, 320 * graph.dataScaleX,
    "The reported hovered region is correct (1).");
  is(graph.getHoveredRegion().end, 460 * graph.dataScaleX,
    "The reported hovered region is correct (2).");

  graph.setSelection({ start: 100, end: 200 });

  info("Setting cursor over the left boundary.");
  graph.setCursor({ x: 100, y: 0 });

  ok(graph._isHoveringStartBoundary(),
    "The graph start boundary should be hovered.");
  ok(!graph._isHoveringEndBoundary(),
    "The graph end boundary should not be hovered.");
  ok(!graph._isHoveringSelectionContents(),
    "The graph contents should not be hovered.");
  ok(graph._isHoveringSelectionContentsOrBoundaries(),
    "The graph contents or boundaries should be hovered.");

  info("Setting cursor near the left boundary.");
  graph.setCursor({ x: 105, y: 0 });

  ok(graph._isHoveringStartBoundary(),
    "The graph start boundary should be hovered.");
  ok(!graph._isHoveringEndBoundary(),
    "The graph end boundary should not be hovered.");
  ok(graph._isHoveringSelectionContents(),
    "The graph contents should be hovered.");
  ok(graph._isHoveringSelectionContentsOrBoundaries(),
    "The graph contents or boundaries should be hovered.");

  info("Setting cursor over the selection.");
  graph.setCursor({ x: 150, y: 0 });

  ok(!graph._isHoveringStartBoundary(),
    "The graph start boundary should not be hovered.");
  ok(!graph._isHoveringEndBoundary(),
    "The graph end boundary should not be hovered.");
  ok(graph._isHoveringSelectionContents(),
    "The graph contents should be hovered.");
  ok(graph._isHoveringSelectionContentsOrBoundaries(),
    "The graph contents or boundaries should be hovered.");

  info("Setting cursor near the right boundary.");
  graph.setCursor({ x: 195, y: 0 });

  ok(!graph._isHoveringStartBoundary(),
    "The graph start boundary should not be hovered.");
  ok(graph._isHoveringEndBoundary(),
    "The graph end boundary should be hovered.");
  ok(graph._isHoveringSelectionContents(),
    "The graph contents should be hovered.");
  ok(graph._isHoveringSelectionContentsOrBoundaries(),
    "The graph contents or boundaries should be hovered.");

  info("Setting cursor over the right boundary.");
  graph.setCursor({ x: 200, y: 0 });

  ok(!graph._isHoveringStartBoundary(),
    "The graph start boundary should not be hovered.");
  ok(graph._isHoveringEndBoundary(),
    "The graph end boundary should be hovered.");
  ok(!graph._isHoveringSelectionContents(),
    "The graph contents should not be hovered.");
  ok(graph._isHoveringSelectionContentsOrBoundaries(),
    "The graph contents or boundaries should be hovered.");

  info("Setting away from the selection.");
  graph.setCursor({ x: 300, y: 0 });

  ok(!graph._isHoveringStartBoundary(),
    "The graph start boundary should not be hovered.");
  ok(!graph._isHoveringEndBoundary(),
    "The graph end boundary should not be hovered.");
  ok(!graph._isHoveringSelectionContents(),
    "The graph contents should not be hovered.");
  ok(!graph._isHoveringSelectionContentsOrBoundaries(),
    "The graph contents or boundaries should not be hovered.");
}
