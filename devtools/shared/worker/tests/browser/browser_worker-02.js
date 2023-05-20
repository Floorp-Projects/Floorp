/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests errors are handled properly by the DevToolsWorker.

const {
  DevToolsWorker,
} = require("resource://devtools/shared/worker/worker.js");

const blob = new Blob(
  [
    `
importScripts("resource://gre/modules/workers/require.js");
const { createTask } = require("resource://devtools/shared/worker/helper.js");

createTask(self, "myTask", function({
  shouldThrow,
} = {}) {
  if (shouldThrow) {
    throw new Error("err");
  }

  return "OK";
});
    `,
  ],
  { type: "application/javascript" }
);

add_task(async function () {
  try {
    new DevToolsWorker("resource://i/dont/exist.js");
    ok(false, "Creating a DevToolsWorker with an invalid URL throws");
  } catch (e) {
    ok(true, "Creating a DevToolsWorker with an invalid URL throws");
  }

  const WORKER_URL = URL.createObjectURL(blob);
  const worker = new DevToolsWorker(WORKER_URL);
  try {
    await worker.performTask("myTask", { shouldThrow: true });
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
    await worker.performTask("myTask");
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

  URL.revokeObjectURL(WORKER_URL);
});
