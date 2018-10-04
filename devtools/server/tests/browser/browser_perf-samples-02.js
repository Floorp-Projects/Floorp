/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the retrieved profiler data samples always have a (root) node.
 * If this ever changes, the |ThreadNode.prototype.insert| function in
 * devtools/client/performance/modules/logic/tree-model.js will have to be changed.
 */

"use strict";

// Time in ms
const WAIT_TIME = 1000;

const { PerformanceFront } = require("devtools/shared/fronts/performance");

add_task(async function() {
  await addTab(MAIN_DOMAIN + "doc_perf.html");

  initDebuggerServer();
  const client = new DebuggerClient(DebuggerServer.connectPipe());
  const form = await connectDebuggerClient(client);
  const front = PerformanceFront(client, form);
  await front.connect();

  const rec = await front.startRecording();
  // allow the profiler module to sample some cpu activity
  busyWait(WAIT_TIME);

  await front.stopRecording(rec);
  const profile = rec.getProfile();
  let sampleCount = 0;

  for (const thread of profile.threads) {
    info("Checking thread: " + thread.name);

    for (const sample of thread.samples.data) {
      sampleCount++;

      const stack = getInflatedStackLocations(thread, sample);
      if (stack[0] != "(root)") {
        ok(false, "The sample " + stack.toSource() + " doesn't have a root node.");
      }
    }
  }

  ok(sampleCount > 0,
    "At least some samples have been iterated over, checking for root nodes.");

  await front.destroy();
  await client.close();
  gBrowser.removeCurrentTab();
});

/**
 * Inflate a particular sample's stack and return an array of strings.
 */
function getInflatedStackLocations(thread, sample) {
  const stackTable = thread.stackTable;
  const frameTable = thread.frameTable;
  const stringTable = thread.stringTable;
  const SAMPLE_STACK_SLOT = thread.samples.schema.stack;
  const STACK_PREFIX_SLOT = stackTable.schema.prefix;
  const STACK_FRAME_SLOT = stackTable.schema.frame;
  const FRAME_LOCATION_SLOT = frameTable.schema.location;

  // Build the stack from the raw data and accumulate the locations in
  // an array.
  let stackIndex = sample[SAMPLE_STACK_SLOT];
  const locations = [];
  while (stackIndex !== null) {
    const stackEntry = stackTable.data[stackIndex];
    const frame = frameTable.data[stackEntry[STACK_FRAME_SLOT]];
    locations.push(stringTable[frame[FRAME_LOCATION_SLOT]]);
    stackIndex = stackEntry[STACK_PREFIX_SLOT];
  }

  // The profiler tree is inverted, so reverse the array.
  return locations.reverse();
}
