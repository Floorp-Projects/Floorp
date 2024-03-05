/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const wdm = Cc["@mozilla.org/dom/workers/workerdebuggermanager;1"].getService(
  Ci.nsIWorkerDebuggerManager
);

const BASE_URL =
  "chrome://mochitests/content/browser/devtools/server/tracer/tests/browser/";
const WORKER_URL = BASE_URL + "Worker.tracer.js";
const WORKER_DEBUGGER_URL = BASE_URL + "WorkerDebugger.tracer.js";

add_task(async function testTracingWorker() {
  const onDbg = waitForWorkerDebugger(WORKER_URL);

  info("Instantiate a regular worker");
  const worker = new Worker(WORKER_URL);
  info("Wait for worker to reply back");
  await new Promise(r => (worker.onmessage = r));
  info("Wait for WorkerDebugger to be instantiated");
  const dbg = await onDbg;

  const onDebuggerScriptSentFrames = new Promise(resolve => {
    const listener = {
      onMessage(msg) {
        dbg.removeListener(listener);
        resolve(JSON.parse(msg));
      },
    };
    dbg.addListener(listener);
  });
  info("Evaluate a Worker Debugger test script");
  dbg.initialize(WORKER_DEBUGGER_URL);

  info("Wait for frames to be notified by the debugger script");
  const frames = await onDebuggerScriptSentFrames;

  is(frames.length, 3);
  // There is a third frame which relates to the usage of Debugger.Object.executeInGlobal
  // which we ignore as that's a test side effect.
  const lastFrame = frames.at(-1);
  const beforeLastFrame = frames.at(-2);
  is(beforeLastFrame.depth, 1);
  is(beforeLastFrame.formatedDisplayName, "λ foo");
  is(beforeLastFrame.prefix, "testWorkerPrefix: ");
  ok(beforeLastFrame.frame);
  is(lastFrame.depth, 2);
  is(lastFrame.formatedDisplayName, "λ bar");
  is(lastFrame.prefix, "testWorkerPrefix: ");
  ok(lastFrame.frame);
});

function waitForWorkerDebugger(url) {
  return new Promise(function (resolve) {
    wdm.addListener({
      onRegister(dbg) {
        if (dbg.url !== url) {
          return;
        }
        ok(true, "Debugger with url " + url + " should be registered.");
        wdm.removeListener(this);
        resolve(dbg);
      },
    });
  });
}
