/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that graph widgets correctly emit mouse input events.

const FAST_FPS = 60;
const SLOW_FPS = 10;

// Each element represents a second
const FRAMES = [FAST_FPS, FAST_FPS, FAST_FPS, SLOW_FPS, FAST_FPS];
const TEST_DATA = [];
const INTERVAL = 100;
const DURATION = 5000;
var t = 0;
for (const frameRate of FRAMES) {
  for (let i = 0; i < frameRate; i++) {
    // Duration between frames at this rate
    const delta = Math.floor(1000 / frameRate);
    t += delta;
    TEST_DATA.push(t);
  }
}

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
  console.log("test data", TEST_DATA);
  await graph.setDataFromTimestamps(TEST_DATA, INTERVAL, DURATION);
  is(graph._avgTooltip.querySelector("[text=value]").textContent, "50",
    "The average tooltip displays the correct value.");
}
