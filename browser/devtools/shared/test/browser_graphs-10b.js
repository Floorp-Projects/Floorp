/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that graphs aren't refreshed when the owner window resizes but
// the graph dimensions stay the same.

const TEST_DATA = [{ delta: 112, value: 48 }, { delta: 213, value: 59 }, { delta: 313, value: 60 }, { delta: 413, value: 59 }, { delta: 530, value: 59 }, { delta: 646, value: 58 }, { delta: 747, value: 60 }, { delta: 863, value: 48 }, { delta: 980, value: 37 }, { delta: 1097, value: 30 }, { delta: 1213, value: 29 }, { delta: 1330, value: 23 }, { delta: 1430, value: 10 }, { delta: 1534, value: 17 }, { delta: 1645, value: 20 }, { delta: 1746, value: 22 }, { delta: 1846, value: 39 }, { delta: 1963, value: 26 }, { delta: 2080, value: 27 }, { delta: 2197, value: 35 }, { delta: 2312, value: 47 }, { delta: 2412, value: 53 }, { delta: 2514, value: 60 }, { delta: 2630, value: 37 }, { delta: 2730, value: 36 }, { delta: 2830, value: 37 }, { delta: 2946, value: 36 }, { delta: 3046, value: 40 }, { delta: 3163, value: 47 }, { delta: 3280, value: 41 }, { delta: 3380, value: 35 }, { delta: 3480, value: 27 }, { delta: 3580, value: 39 }, { delta: 3680, value: 42 }, { delta: 3780, value: 49 }, { delta: 3880, value: 55 }, { delta: 3980, value: 60 }, { delta: 4080, value: 60 }, { delta: 4180, value: 60 }];
var LineGraphWidget = require("devtools/shared/widgets/LineGraphWidget");

add_task(function*() {
  yield promiseTab("about:blank");
  yield performTest();
  gBrowser.removeCurrentTab();
});

function* performTest() {
  let [host, win, doc] = yield createHost("window");
  doc.body.setAttribute("style", "position: fixed; width: 100%; height: 100%; margin: 0;");

  let graph = new LineGraphWidget(doc.body, "fps");
  graph.fixedWidth = 200;
  graph.fixedHeight = 100;
  yield graph.once("ready");

  let refreshCount = 0;
  let refreshCancelledCount = 0;
  graph.on("refresh", () => refreshCount++);
  graph.on("refresh-cancelled", () => refreshCancelledCount++);

  yield testGraph(host, graph);

  is(refreshCount, 0, "The graph shouldn't have been refreshed at all.");
  is(refreshCancelledCount, 2, "The graph should've had 2 refresh attempts.");

  yield graph.destroy();
  host.destroy();
}

function* testGraph(host, graph) {
  graph.setData(TEST_DATA);

  host._window.resizeBy(-100, -100);
  yield graph.once("refresh-cancelled");

  host._window.resizeBy(100, 100);
  yield graph.once("refresh-cancelled");
}
