/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests if clicking on regions adds a selection spanning that region.

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

add_task(function* () {
  yield addTab("about:blank");
  yield performTest();
  gBrowser.removeCurrentTab();
});

function* performTest() {
  let [host,, doc] = yield createHost();
  let graph = new LineGraphWidget(doc.body, "fps");
  yield graph.once("ready");

  testGraph(graph);

  yield graph.destroy();
  host.destroy();
}

function testGraph(graph) {
  graph.setData(TEST_DATA);
  graph.setRegions(TEST_REGIONS);

  click(graph, (graph._regions[0].start + graph._regions[0].end) / 2);
  is(graph.getSelection().start, graph._regions[0].start,
    "The first region is now selected (1).");
  is(graph.getSelection().end, graph._regions[0].end,
    "The first region is now selected (2).");

  let min = map(graph.getSelection().start, 0, graph.width, 112, 4180);
  let max = map(graph.getSelection().end, 0, graph.width, 112, 4180);
  is(graph.getMappedSelection().min, min,
    "The mapped selection's min value is correct (1).");
  is(graph.getMappedSelection().max, max,
    "The mapped selection's max value is correct (2).");

  click(graph, (graph._regions[1].start + graph._regions[1].end) / 2);
  is(graph.getSelection().start, graph._regions[1].start,
    "The second region is now selected (1).");
  is(graph.getSelection().end, graph._regions[1].end,
    "The second region is now selected (2).");

  min = map(graph.getSelection().start, 0, graph.width, 112, 4180);
  max = map(graph.getSelection().end, 0, graph.width, 112, 4180);
  is(graph.getMappedSelection().min, min,
    "The mapped selection's min value is correct (3).");
  is(graph.getMappedSelection().max, max,
    "The mapped selection's max value is correct (4).");

  graph.setSelection({ start: graph.width, end: 0 });
  min = map(0, 0, graph.width, 112, 4180);
  max = map(graph.width, 0, graph.width, 112, 4180);
  is(graph.getMappedSelection().min, min,
    "The mapped selection's min value is correct (5).");
  is(graph.getMappedSelection().max, max,
    "The mapped selection's max value is correct (6).");

  graph.setSelection({ start: graph.width + 100, end: -100 });
  min = map(0, 0, graph.width, 112, 4180);
  max = map(graph.width, 0, graph.width, 112, 4180);
  is(graph.getMappedSelection().min, min,
    "The mapped selection's min value is correct (7).");
  is(graph.getMappedSelection().max, max,
    "The mapped selection's max value is correct (8).");
}

/**
 * Maps a value from one range to another.
 */
function map(value, istart, istop, ostart, ostop) {
  return ostart + (ostop - ostart) * ((value - istart) / (istop - istart));
}

// EventUtils just doesn't work!

function click(graph, x, y = 1) {
  x /= window.devicePixelRatio;
  y /= window.devicePixelRatio;
  graph._onMouseMove({ testX: x, testY: y });
  graph._onMouseDown({ testX: x, testY: y });
  graph._onMouseUp({ testX: x, testY: y });
}
