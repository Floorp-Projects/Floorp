/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the devtools/shared/worker communicates properly
// as both CommonJS module and as a JSM.

const WORKER_URL = "resource://devtools/client/shared/widgets/GraphsWorker.js";

const BUFFER_SIZE = 8;
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

  await testWorker("JSM", () =>
    ChromeUtils.import("resource://devtools/shared/worker/worker.js", null)
  );
  await testWorker("CommonJS", () => require("devtools/shared/worker/worker"));
  await testTransfer();
});

async function testWorker(context, workerFactory) {
  const { DevToolsWorker, workerify } = workerFactory();
  const worker = new DevToolsWorker(WORKER_URL);
  const results = await worker.performTask("plotTimestampsGraph", {
    timestamps: WORKER_DATA,
    interval: INTERVAL,
    duration: DURATION,
  });

  ok(
    results.plottedData.length,
    `worker should have returned an object with array properties in ${context}`
  );

  const fn = workerify(x => x * x);
  is(await fn(5), 25, `workerify works in ${context}`);
  fn.destroy();

  worker.destroy();
}

async function testTransfer() {
  const { workerify } = ChromeUtils.import(
    "resource://devtools/shared/worker/worker.js",
    null
  );
  const workerFn = workerify(({ buf }) => buf.byteLength);
  const buf = new ArrayBuffer(BUFFER_SIZE);

  is(
    buf.byteLength,
    BUFFER_SIZE,
    "Size of the buffer before transfer is correct."
  );

  is(await workerFn({ buf }), 8, "Sent array buffer to worker");
  is(buf.byteLength, 8, "Array buffer was copied, not transferred.");

  is(await workerFn({ buf }, [buf]), 8, "Sent array buffer to worker");
  is(buf.byteLength, 0, "Array buffer was transferred, not copied.");

  workerFn.destroy();
}
