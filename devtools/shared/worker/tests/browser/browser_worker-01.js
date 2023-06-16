/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the devtools/shared/worker communicates properly
// as both CommonJS module and as a JSM.

const BUFFER_SIZE = 8;

registerCleanupFunction(function () {
  Services.prefs.clearUserPref("security.allow_parent_unrestricted_js_loads");
});

add_task(async function () {
  // Test both CJS and JSM versions

  await testWorker("JSM", () =>
    ChromeUtils.import("resource://devtools/shared/worker/worker.js")
  );
  await testWorker("CommonJS", () =>
    require("resource://devtools/shared/worker/worker.js")
  );
  await testTransfer();
});

async function testWorker(context, workerFactory) {
  // Needed for blob:null
  Services.prefs.setBoolPref(
    "security.allow_parent_unrestricted_js_loads",
    true
  );
  const { DevToolsWorker, workerify } = workerFactory();

  const blob = new Blob(
    [
      `
importScripts("resource://gre/modules/workers/require.js");
const { createTask } = require("resource://devtools/shared/worker/helper.js");

createTask(self, "groupByField", function({
  items,
  groupField
}) {
  const groups = {};
  for (const item of items) {
    if (!groups[item[groupField]]) {
      groups[item[groupField]] = [];
    }
    groups[item[groupField]].push(item);
  }
  return { groups };
});
    `,
    ],
    { type: "application/javascript" }
  );

  const WORKER_URL = URL.createObjectURL(blob);
  const worker = new DevToolsWorker(WORKER_URL);

  const results = await worker.performTask("groupByField", {
    items: [
      { name: "Paris", country: "France" },
      { name: "Lagos", country: "Nigeria" },
      { name: "Lyon", country: "France" },
    ],
    groupField: "country",
  });

  is(
    Object.keys(results.groups).join(","),
    "France,Nigeria",
    `worker should have returned the expected result in ${context}`
  );

  URL.revokeObjectURL(WORKER_URL);

  const fn = workerify(x => x * x);
  is(await fn(5), 25, `workerify works in ${context}`);
  fn.destroy();

  worker.destroy();
}

async function testTransfer() {
  Services.prefs.setBoolPref(
    "security.allow_parent_unrestricted_js_loads",
    true
  );
  const { workerify } = ChromeUtils.import(
    "resource://devtools/shared/worker/worker.js"
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
