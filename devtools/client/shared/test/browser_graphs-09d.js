/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that line graphs hide the 'max' tooltip when the distance between
// the 'min' and 'max' tooltip is too small.

const TEST_DATA = [{ delta: 100, value: 60 }, { delta: 200, value: 59.9 }];
const LineGraphWidget = require("devtools/client/shared/widgets/LineGraphWidget");

add_task(async function() {
  await addTab("about:blank");
  await performTest();
  gBrowser.removeCurrentTab();
});

async function performTest() {
  const [host,, doc] = await createHost();
  const graph = new LineGraphWidget(doc.body, "fps");

  await testGraph(graph);

  await graph.destroy();
  host.destroy();
}

async function testGraph(graph) {
  await graph.setDataWhenReady(TEST_DATA);

  is(graph._gutter.hidden, false,
    "The gutter should not be hidden.");
  is(graph._maxTooltip.hidden, true,
    "The max tooltip should be hidden.");
  is(graph._avgTooltip.hidden, false,
    "The avg tooltip should not be hidden.");
  is(graph._minTooltip.hidden, false,
    "The min tooltip should not be hidden.");
}
