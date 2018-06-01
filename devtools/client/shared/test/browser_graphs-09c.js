/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that line graphs hide the tooltips when there's no data available.

const TEST_DATA = [];
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

  is(graph._gutter.hidden, true,
    "The gutter should be hidden, since there's no data available.");
  is(graph._maxTooltip.hidden, true,
    "The max tooltip should be hidden.");
  is(graph._avgTooltip.hidden, true,
    "The avg tooltip should be hidden.");
  is(graph._minTooltip.hidden, true,
    "The min tooltip should be hidden.");
}
