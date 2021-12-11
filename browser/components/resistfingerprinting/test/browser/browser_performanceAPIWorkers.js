/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// ================================================================================================
// ================================================================================================
let runWorkerTest = async function(data) {
  let expectedPrecision = data.precision;
  let workerCall = data.workerCall;
  await new Promise(resolve => {
    let worker = new content.Worker("file_workerPerformance.js");
    worker.onmessage = function(e) {
      if (e.data.type == "status") {
        ok(e.data.status, e.data.msg);
      } else if (e.data.type == "finish") {
        worker.terminate();
        resolve();
      } else {
        ok(false, "Unknown message type");
        worker.terminate();
        resolve();
      }
    };
    worker.postMessage({ type: workerCall, precision: expectedPrecision });
  });
};

add_task(async function runRPTestsForWorker() {
  await setupPerformanceAPISpoofAndDisableTest(
    true,
    true,
    false,
    100,
    runWorkerTest,
    "runRPTests"
  );
  await setupPerformanceAPISpoofAndDisableTest(
    true,
    false,
    false,
    13,
    runWorkerTest,
    "runRPTests"
  );
  await setupPerformanceAPISpoofAndDisableTest(
    true,
    true,
    false,
    0.13,
    runWorkerTest,
    "runRPTests"
  );
});

add_task(async function runRTPTestsForWorker() {
  await setupPerformanceAPISpoofAndDisableTest(
    false,
    true,
    false,
    100,
    runWorkerTest,
    "runRTPTests"
  );
  await setupPerformanceAPISpoofAndDisableTest(
    false,
    true,
    false,
    13,
    runWorkerTest,
    "runRTPTests"
  );
  await setupPerformanceAPISpoofAndDisableTest(
    false,
    true,
    false,
    0.13,
    runWorkerTest,
    "runRTPTests"
  );
});
