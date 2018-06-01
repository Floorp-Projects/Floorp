/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that mounta graphs work as expected.

const MountainGraphWidget = require("devtools/client/shared/widgets/MountainGraphWidget");

const TEST_DATA = [
  { delta: 0, values: [0.1, 0.5, 0.3] },
  { delta: 1, values: [0.25, 0, 0.5] },
  { delta: 2, values: [0.5, 0.25, 0.1] },
  { delta: 3, values: [0, 0.75, 0] },
  { delta: 4, values: [0.75, 0, 0.25] }
];

const SECTIONS = [
  { color: "red" },
  { color: "green" },
  { color: "blue" }
];

add_task(async function() {
  await addTab("about:blank");
  await performTest();
  gBrowser.removeCurrentTab();
});

async function performTest() {
  const [host,, doc] = await createHost();
  const graph = new MountainGraphWidget(doc.body);
  await graph.once("ready");

  testGraph(graph);

  await graph.destroy();
  host.destroy();
}

function testGraph(graph) {
  graph.format = SECTIONS;
  graph.setData(TEST_DATA);
  ok(true, "The graph didn't throw any erorrs.");
}
