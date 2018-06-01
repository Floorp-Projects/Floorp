/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the devtools/shared/worker communicates properly
// as both CommonJS module and as a JSM.

const WORKER_URL =
  "resource://devtools/client/shared/widgets/GraphsWorker.js";

const count = 100000;
const WORKER_DATA = (function() {
  const timestamps = [];
  for (let i = 0; i < count; i++) {
    timestamps.push(i);
  }
  return timestamps;
})();
const INTERVAL = 100;
const DURATION = 1000;

add_task(async function() {
  // Test both CJS and JSM versions

  await testWorker("JSM", () => ChromeUtils.import("resource://devtools/shared/worker/worker.js", {}));
  await testWorker("CommonJS", () => require("devtools/shared/worker/worker"));
});

async function testWorker(context, workerFactory) {
  const { DevToolsWorker, workerify } = workerFactory();
  const worker = new DevToolsWorker(WORKER_URL);
  const results = await worker.performTask("plotTimestampsGraph", {
    timestamps: WORKER_DATA,
    interval: INTERVAL,
    duration: DURATION
  });

  ok(results.plottedData.length,
    `worker should have returned an object with array properties in ${context}`);

  const fn = workerify(x => x * x);
  is((await fn(5)), 25, `workerify works in ${context}`);
  fn.destroy();

  worker.destroy();
}
