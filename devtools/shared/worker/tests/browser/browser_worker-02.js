/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests errors are handled properly by the DevToolsWorker.

const { DevToolsWorker } = require("devtools/shared/worker/worker");
const WORKER_URL = "resource://devtools/client/shared/widgets/GraphsWorker.js";

add_task(async function() {
  try {
    new DevToolsWorker("resource://i/dont/exist.js");
    ok(false, "Creating a DevToolsWorker with an invalid URL throws");
  } catch (e) {
    ok(true, "Creating a DevToolsWorker with an invalid URL throws");
  }

  const worker = new DevToolsWorker(WORKER_URL);
  try {
    // plotTimestampsGraph requires timestamp, interval an duration props on the object
    // passed in so there should be an error thrown in the worker
    await worker.performTask("plotTimestampsGraph", {});
    ok(
      false,
      "DevToolsWorker returns a rejected promise when an error occurs in the worker"
    );
  } catch (e) {
    ok(
      true,
      "DevToolsWorker returns a rejected promise when an error occurs in the worker"
    );
  }

  try {
    await worker.performTask("not a real task");
    ok(
      false,
      "DevToolsWorker returns a rejected promise when task does not exist"
    );
  } catch (e) {
    ok(
      true,
      "DevToolsWorker returns a rejected promise when task does not exist"
    );
  }

  worker.destroy();
  try {
    await worker.performTask("plotTimestampsGraph", {
      timestamps: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9],
      interval: 1,
      duration: 1,
    });
    ok(
      false,
      "DevToolsWorker rejects when performing a task on a destroyed worker"
    );
  } catch (e) {
    ok(
      true,
      "DevToolsWorker rejects when performing a task on a destroyed worker"
    );
  }
});
