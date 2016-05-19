/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that line graphs hide the 'max' tooltip when the distance between
// the 'min' and 'max' tooltip is too small.

const TEST_DATA = [{ delta: 100, value: 60 }, { delta: 200, value: 59.9 }];
var LineGraphWidget = require("devtools/client/shared/widgets/LineGraphWidget");

add_task(function* () {
  yield addTab("about:blank");
  yield performTest();
  gBrowser.removeCurrentTab();
});

function* performTest() {
  let [host, win, doc] = yield createHost();
  let graph = new LineGraphWidget(doc.body, "fps");

  yield testGraph(graph);

  yield graph.destroy();
  host.destroy();
}

function* testGraph(graph) {
  yield graph.setDataWhenReady(TEST_DATA);

  is(graph._gutter.hidden, false,
    "The gutter should not be hidden.");
  is(graph._maxTooltip.hidden, true,
    "The max tooltip should be hidden.");
  is(graph._avgTooltip.hidden, false,
    "The avg tooltip should not be hidden.");
  is(graph._minTooltip.hidden, false,
    "The min tooltip should not be hidden.");
}
