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

add_task(function* () {
  yield addTab(MAIN_DOMAIN + "doc_perf.html");

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);
  let front = PerformanceFront(client, form);
  yield front.connect();

  let rec = yield front.startRecording();
  // allow the profiler module to sample some cpu activity
  busyWait(WAIT_TIME);

  yield front.stopRecording(rec);
  let profile = rec.getProfile();
  let sampleCount = 0;

  for (let thread of profile.threads) {
    info("Checking thread: " + thread.name);

    for (let sample of thread.samples.data) {
      sampleCount++;

      let stack = getInflatedStackLocations(thread, sample);
      if (stack[0] != "(root)") {
        ok(false, "The sample " + stack.toSource() + " doesn't have a root node.");
      }
    }
  }

  ok(sampleCount > 0,
    "At least some samples have been iterated over, checking for root nodes.");

  yield front.destroy();
  yield client.close();
  gBrowser.removeCurrentTab();
});

/**
 * Inflate a particular sample's stack and return an array of strings.
 */
function getInflatedStackLocations(thread, sample) {
  let stackTable = thread.stackTable;
  let frameTable = thread.frameTable;
  let stringTable = thread.stringTable;
  let SAMPLE_STACK_SLOT = thread.samples.schema.stack;
  let STACK_PREFIX_SLOT = stackTable.schema.prefix;
  let STACK_FRAME_SLOT = stackTable.schema.frame;
  let FRAME_LOCATION_SLOT = frameTable.schema.location;

  // Build the stack from the raw data and accumulate the locations in
  // an array.
  let stackIndex = sample[SAMPLE_STACK_SLOT];
  let locations = [];
  while (stackIndex !== null) {
    let stackEntry = stackTable.data[stackIndex];
    let frame = frameTable.data[stackEntry[STACK_FRAME_SLOT]];
    locations.push(stringTable[frame[FRAME_LOCATION_SLOT]]);
    stackIndex = stackEntry[STACK_PREFIX_SLOT];
  }

  // The profiler tree is inverted, so reverse the array.
  return locations.reverse();
}
