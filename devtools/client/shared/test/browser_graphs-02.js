/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that graph widgets can properly add data, regions and highlights.

const TEST_DATA = [
  { delta: 112, value: 48 }, { delta: 213, value: 59 },
  { delta: 313, value: 60 }, { delta: 413, value: 59 },
  { delta: 530, value: 59 }, { delta: 646, value: 58 },
  { delta: 747, value: 60 }, { delta: 863, value: 48 },
  { delta: 980, value: 37 }, { delta: 1097, value: 30 },
  { delta: 1213, value: 29 }, { delta: 1330, value: 23 },
  { delta: 1430, value: 10 }, { delta: 1534, value: 17 },
  { delta: 1645, value: 20 }, { delta: 1746, value: 22 },
  { delta: 1846, value: 39 }, { delta: 1963, value: 26 },
  { delta: 2080, value: 27 }, { delta: 2197, value: 35 },
  { delta: 2312, value: 47 }, { delta: 2412, value: 53 },
  { delta: 2514, value: 60 }, { delta: 2630, value: 37 },
  { delta: 2730, value: 36 }, { delta: 2830, value: 37 },
  { delta: 2946, value: 36 }, { delta: 3046, value: 40 },
  { delta: 3163, value: 47 }, { delta: 3280, value: 41 },
  { delta: 3380, value: 35 }, { delta: 3480, value: 27 },
  { delta: 3580, value: 39 }, { delta: 3680, value: 42 },
  { delta: 3780, value: 49 }, { delta: 3880, value: 55 },
  { delta: 3980, value: 60 }, { delta: 4080, value: 60 },
  { delta: 4180, value: 60 }
];
const TEST_REGIONS = [{ start: 320, end: 460 }, { start: 780, end: 860 }];
const LineGraphWidget = require("devtools/client/shared/widgets/LineGraphWidget");

add_task(async function() {
  await addTab("about:blank");
  await performTest();
  gBrowser.removeCurrentTab();
});

async function performTest() {
  const [host,, doc] = await createHost();
  const graph = new LineGraphWidget(doc.body, "fps");
  await graph.once("ready");

  testDataAndRegions(graph);
  testHighlights(graph);

  await graph.destroy();
  host.destroy();
}

function testDataAndRegions(graph) {
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
     // last & first tick in TEST_DATA
     graph.width / 4180,
    "The data scale on the X axis is correct.");

  is(graph.dataScaleY,
     // max value in TEST_DATA * GRAPH_DAMPEN_VALUES
     graph.height / 60 * 0.85,
    "The data scale on the Y axis is correct.");

  for (let i = 0; i < TEST_REGIONS.length; i++) {
    const original = TEST_REGIONS[i];
    const normalized = graph._regions[i];

    is(original.start * graph.dataScaleX, normalized.start,
      "The region's start value was properly normalized.");
    is(original.end * graph.dataScaleX, normalized.end,
      "The region's end value was properly normalized.");
  }
}

function testHighlights(graph) {
  graph.setMask(TEST_REGIONS);
  ok(graph.hasMask(),
    "The graph should now have the highlights set.");

  graph.setMask([]);
  ok(graph.hasMask(),
    "The graph shouldn't have anything highlighted.");

  graph.setMask(null);
  ok(!graph.hasMask(),
    "The graph should have everything highlighted.");
}
