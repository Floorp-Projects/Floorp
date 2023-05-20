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
let runWorkerTest = async function (data) {
  let expectedPrecision = data.precision;
  await new Promise(resolve => {
    // eslint beleives that isrounded is available in this scope, but if you
    // remove the assignment, you will see it is not
    // eslint-disable-next-line
    let isRounded = eval(data.isRoundedFunc);

    let worker = new content.Worker(
      "coop_header.sjs?crossOriginIsolated=true&worker=true"
    );

    worker.postMessage({
      type: "runCmdAndGetResult",
      cmd: `performance.timeOrigin`,
    });

    const expectedAllEntriesLength = 3;
    const expectedResourceEntriesLength = 2;
    const expectedTestAndMarkEntriesLength = 1;

    worker.onmessage = function (e) {
      if (e.data.type == "result") {
        if (e.data.resultOf == "performance.timeOrigin") {
          ok(
            isRounded(e.data.result, expectedPrecision),
            `In a worker, performance.timeOrigin is` +
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
            `In a worker: Incorrect number of ` +
              `entries for performance.getEntries() for workers: ` +
              `${e.data.result}`
          );

          worker.postMessage({
            type: "getResult",
            resultOf: "startTimeAndDuration",
            num: 0,
          });
        } else if (e.data.resultOf == "startTimeAndDuration") {
          let index = e.data.result.index;
          let startTime = e.data.result.startTime;
          let duration = e.data.result.duration;
          ok(
            isRounded(startTime, expectedPrecision),
            `In a worker, for precision(${expectedPrecision}, ` +
              `performance.getEntries(${index}).startTime is not rounded: ` +
              `${startTime}`
          );
          ok(
            isRounded(duration, expectedPrecision),
            `In a worker, for precision(${expectedPrecision}), ` +
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
            `In a worker: Incorrect number of ` +
              `entries for performance.getEntriesByType() for workers: ` +
              `${e.data.result.resourceLength}`
          );
          is(
            e.data.result.testAndMarkLength,
            expectedTestAndMarkEntriesLength,
            `In a worker: Incorrect number of ` +
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
    {
      resistFingerprinting: true,
      reduceTimerPrecision: true,
      crossOriginIsolated: true,
    },
    100,
    runWorkerTest,
    "runTimerTests"
  );
  await setupAndRunCrossOriginIsolatedTest(
    {
      resistFingerprinting: true,
      crossOriginIsolated: true,
    },
    13,
    runWorkerTest,
    "runTimerTests"
  );
  await setupAndRunCrossOriginIsolatedTest(
    {
      resistFingerprinting: true,
      reduceTimerPrecision: true,
      crossOriginIsolated: true,
    },
    0.13,
    runWorkerTest,
    "runTimerTests"
  );

  // RTP
  await setupAndRunCrossOriginIsolatedTest(
    {
      reduceTimerPrecision: true,
    },
    0.13,
    runWorkerTest,
    "runTimerTests"
  );
  await setupAndRunCrossOriginIsolatedTest(
    {
      reduceTimerPrecision: true,
      crossOriginIsolated: true,
    },
    0.005,
    runWorkerTest,
    "runTimerTests"
  );
});
