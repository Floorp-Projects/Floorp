/**
 * Bug 1621677 - A test for making sure getting the correct (higher) precision
 *   when it's cross-origin-isolated for performance APIs.
 */

// ================================================================================================
// ================================================================================================
// This test case is mostly copy-and-paste from the forth test case in
// browser_performanceAPI.js. The main difference is this test case verifies
// performance API have more precsion when it's in cross-origin-isolated and
// cross-origin-isolated doesn't affect RFP.
let runWorkerTest = async function(data) {
  let expectedPrecision = data.precision;
  await new Promise(resolve => {
    var isRounded = function() {
      return "Placeholder for eslint";
    };
    let evalStr = "var isRounded = " + data.isRoundedFunc;
    // eslint-disable-next-line
    eval(evalStr);

    let worker = new content.Worker(
      "coop_header.sjs?crossOriginIsolated=true&worker=true"
    );

    worker.postMessage({
      type: "runCmdAndGetResult",
      cmd: `performance.timeOrigin`,
    });

    const expectedAllEntriesLength = data.workerCall == "runRPTests" ? 0 : 3;
    const expectedResourceEntriesLength =
      data.workerCall == "runRPTests" ? 0 : 2;
    const expectedTestAndMarkEntriesLength =
      data.workerCall == "runRPTests" ? 0 : 1;

    worker.onmessage = function(e) {
      if (e.data.type == "result") {
        if (e.data.resultOf == "performance.timeOrigin") {
          ok(
            isRounded(e.data.result, expectedPrecision),
            `In a worker, for reduceTimerPrecision, performance.timeOrigin is` +
              ` not correctly rounded: ${e.data.result}`
          );

          worker.postMessage({
            type: "runCmds",
            cmds: [
              `performance.mark("Test");`,
              `performance.mark("Test-End");`,
              `performance.measure("Test-Measure", "Test", "Test-End");`,
            ],
          });
        } else if (e.data.resultOf == "entriesLength") {
          is(
            e.data.result,
            expectedAllEntriesLength,
            `In a worker, for reduceTimerPrecision: Incorrect number of ` +
              `entries for performance.getEntries() for workers: ` +
              `${e.data.result}`
          );

          if (data.workerCall == "runRTPTests") {
            worker.postMessage({
              type: "getResult",
              resultOf: "startTimeAndDuration",
              num: 0,
            });
          } else {
            worker.postMessage({
              type: "getResult",
              resultOf: "getEntriesByTypeLength",
            });
          }
        } else if (e.data.resultOf == "startTimeAndDuration") {
          let index = e.data.result.index;
          let startTime = e.data.result.startTime;
          let duration = e.data.result.duration;
          ok(
            isRounded(startTime, expectedPrecision),
            `In a worker, for reduceTimerPrecision(${expectedPrecision}, ` +
              `performance.getEntries(${index}).startTime is not rounded: ` +
              `${startTime}`
          );
          ok(
            isRounded(duration, expectedPrecision),
            `In a worker, for reduceTimerPrecision(${expectedPrecision}), ` +
              `performance.getEntries(${index}).duration is not rounded: ` +
              `${duration}`
          );

          if (index < 2) {
            worker.postMessage({
              type: "getResult",
              resultOf: "startTimeAndDuration",
              num: index + 1,
            });
          } else {
            worker.postMessage({
              type: "getResult",
              resultOf: "getEntriesByTypeLength",
            });
          }
        } else if (e.data.resultOf == "entriesByTypeLength") {
          is(
            e.data.result.markLength,
            expectedResourceEntriesLength,
            `In a worker, for reduceTimerPrecision: Incorrect number of ` +
              `entries for performance.getEntriesByType() for workers: ` +
              `${e.data.result.resourceLength}`
          );
          is(
            e.data.result.testAndMarkLength,
            expectedTestAndMarkEntriesLength,
            `In a worker, for reduceTimerPrecision: Incorrect number of ` +
              `entries for performance.getEntriesByName() for workers: ` +
              `${e.data.result.testAndMarkLength}`
          );

          worker.terminate();
          resolve();
        }
      } else {
        ok(false, `Unknown message type got ${e.data.type}`);
        worker.terminate();
        resolve();
      }
    };
  });
};

add_task(async function runTestsForWorker() {
  // RFP
  await setupAndRunCrossOriginIsolatedTest(
    true,
    true,
    true,
    100,
    runWorkerTest,
    "runRPTests"
  );
  await setupAndRunCrossOriginIsolatedTest(
    true,
    false,
    true,
    13,
    runWorkerTest,
    "runRPTests"
  );
  await setupAndRunCrossOriginIsolatedTest(
    true,
    true,
    true,
    0.13,
    runWorkerTest,
    "runRPTests"
  );

  // RTP
  await setupAndRunCrossOriginIsolatedTest(
    false,
    true,
    false,
    0.13,
    runWorkerTest,
    "runRTPTests"
  );
  await setupAndRunCrossOriginIsolatedTest(
    false,
    true,
    true,
    0.005,
    runWorkerTest,
    "runRTPTests"
  );
});
