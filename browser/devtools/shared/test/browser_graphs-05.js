/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that graph widgets can correctly determine which regions are hovered.

const TEST_DATA = [{ delta: 112, value: 48 }, { delta: 213, value: 59 }, { delta: 313, value: 60 }, { delta: 413, value: 59 }, { delta: 530, value: 59 }, { delta: 646, value: 58 }, { delta: 747, value: 60 }, { delta: 863, value: 48 }, { delta: 980, value: 37 }, { delta: 1097, value: 30 }, { delta: 1213, value: 29 }, { delta: 1330, value: 23 }, { delta: 1430, value: 10 }, { delta: 1534, value: 17 }, { delta: 1645, value: 20 }, { delta: 1746, value: 22 }, { delta: 1846, value: 39 }, { delta: 1963, value: 26 }, { delta: 2080, value: 27 }, { delta: 2197, value: 35 }, { delta: 2312, value: 47 }, { delta: 2412, value: 53 }, { delta: 2514, value: 60 }, { delta: 2630, value: 37 }, { delta: 2730, value: 36 }, { delta: 2830, value: 37 }, { delta: 2946, value: 36 }, { delta: 3046, value: 40 }, { delta: 3163, value: 47 }, { delta: 3280, value: 41 }, { delta: 3380, value: 35 }, { delta: 3480, value: 27 }, { delta: 3580, value: 39 }, { delta: 3680, value: 42 }, { delta: 3780, value: 49 }, { delta: 3880, value: 55 }, { delta: 3980, value: 60 }, { delta: 4080, value: 60 }, { delta: 4180, value: 60 }];
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
