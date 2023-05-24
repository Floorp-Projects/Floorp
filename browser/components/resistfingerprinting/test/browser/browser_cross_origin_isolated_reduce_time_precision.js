/**
 * Bug 1621677 - A test for making sure getting the correct (higher) precision
 *   when it's cross-origin-isolated.
 */

const TEST_SCENARIO_1 = 1;
const TEST_SCENARIO_2 = 2;
const TEST_SCENARIO_3 = 3;
const TEST_SCENARIO_4 = 4;
const TEST_SCENARIO_5 = 5;
const TEST_SCENARIO_6 = 6;
const TEST_SCENARIO_7 = 7;
const TEST_SCENARIO_8 = 8;
const TEST_SCENARIO_9 = 9;
const TEST_SCENARIO_10 = 10;
const TEST_SCENARIO_11 = 11;

const TEST_SCENARIO_101 = 101;
const TEST_SCENARIO_102 = 102;
const TEST_SCENARIO_103 = 103;
const TEST_SCENARIO_104 = 104;
const TEST_SCENARIO_105 = 105;
const TEST_SCENARIO_106 = 106;
const TEST_SCENARIO_107 = 107;
const TEST_SCENARIO_108 = 108;
const TEST_SCENARIO_109 = 109;
const TEST_SCENARIO_110 = 110;
const TEST_SCENARIO_111 = 111;

requestLongerTimeout(2);

let processResultsGlobal = (data, successes, failures) => {
  let expectedPrecision = data.precision;
  let scenario = data.options.scenario;
  let shouldBeRounded = data.options.shouldBeRounded;
  for (let success of successes) {
    ok(
      true,
      (shouldBeRounded ? "Should " : "Should not ") +
        `have rounded '${success[0]}' to nearest ${expectedPrecision} ms; saw ${success[1]}. ` +
        `scenario: TEST_SCENARIO_${scenario}`
    );
  }
  if (failures.length > 2) {
    for (let failure of failures) {
      ok(
        false,
        (shouldBeRounded ? "Should " : "Should not ") +
          `have rounded '${failure[0]}' to nearest ${expectedPrecision} ms; saw ${failure[1]}. ` +
          `scenario: TEST_SCENARIO_${scenario}`
      );
    }
  } else if (
    failures.length == 2 &&
    expectedPrecision < 10 &&
    failures[0][0].indexOf("Date().getTime()") > 0 &&
    failures[1][0].indexOf('File([], "").lastModified') > 0
  ) {
    /*
     * At high precisions, the epoch-based timestamps are large enough that their expected
     * rounding values lie directly between two integers; and floating point math is imprecise enough
     * that we need to accept these failures
     */
    ok(
      true,
      "Two Free Failures that " +
        (data.options.shouldBeRounded ? "ahould " : "should not ") +
        `be rounded on the epoch dates and precision: ${expectedPrecision}. ` +
        `scenario: TEST_SCENARIO_${data.options.scenario}`
    );
  } else if (failures.length == 1) {
    ok(
      true,
      "Free Failure: " +
        (data.options.shouldBeRounded ? "Should " : "Should not ") +
        `have rounded '${failures[0][0]}' to nearest ${expectedPrecision} ms; saw ${failures[0][1]}. ` +
        `scenario: TEST_SCENARIO_${data.options.scenario}`
    );
  }
};

// ================================================================================================
// ================================================================================================
// This test case is mostly copy-and-paste from the test case for window in
// test_reduce_time_precision.html. The main difference is this test case
// verifies DOM API has more precsion when it's in cross-origin-isolated and
// cross-origin-isolated doesn't affect RFP.
add_task(async function runRTPTestDOM() {
  let runTests = async function (data) {
    let expectedPrecision = data.precision;
    // eslint beleives that isrounded is available in this scope, but if you
    // remove the assignment, you will see it is not
    // eslint-disable-next-line
    let isRounded = eval(data.isRoundedFunc);
    // eslint-disable-next-line
    let processResults = eval(data.options.processResultsFunc);

    // Prepare for test of AudioContext.currentTime
    // eslint-disable-next-line
    let audioContext = new content.AudioContext();

    // Known ways to generate time stamps, in milliseconds
    const timeStampCodes = [
      "new content.Date().getTime()",
      'new content.File([], "").lastModified',
      "content.performance.now()",
      'new content.Event("").timeStamp',
    ];
    // These are measured in seconds, so we need to scale them up
    var timeStampCodesDOM = timeStampCodes.concat([
      "audioContext.currentTime * 1000",
    ]);

    // If we are not rounding values, this function will invert the return value
    let resultSwitchisRounded = function (timeStamp) {
      if (timeStamp == 0) {
        return true;
      }
      let result = isRounded(timeStamp, expectedPrecision, content.console);
      return data.options.shouldBeRounded ? result : !result;
    };

    // It's possible that even if we shouldn't be rounding something, we get a timestamp that is rounded.
    // If we have only one of these outliers, we are okay with that.  This should significantly
    // reduce intermittents, although it's still likely there will be some intermittents.  We can increase
    // this if we need to
    let successes = [],
      failures = [];

    // Loop through each timeStampCode, evaluate it,
    // and check if it is rounded
    for (let timeStampCode of timeStampCodesDOM) {
      // eslint-disable-next-line
      let timeStamp = eval(timeStampCode);

      // Audio Contexts increment in intervals of (minimum) 5.4ms, so we don't
      // clamp/jitter if the timer precision is les than that.
      // (Technically on MBPs they increment in intervals of 2.6 but this is
      // non-standard and will eventually be changed. We don't cover this situation
      // because we don't really support arbitrary Timer Precision, especially in
      // the 2.6 - 5.4ms interval.)
      if (timeStampCode.includes("audioContext") && expectedPrecision < 5.4) {
        continue;
      }

      if (timeStamp != 0 && resultSwitchisRounded(timeStamp)) {
        successes = successes.concat([[timeStampCode, timeStamp]]);
      } else if (timeStamp != 0) {
        failures = failures.concat([[timeStampCode, timeStamp]]);
      }
    }

    processResults(data, successes, failures);
  };

  // RFP
  await setupAndRunCrossOriginIsolatedTest(
    {
      resistFingerprinting: true,
      reduceTimerPrecision: true,
      crossOriginIsolated: true,
      scenario: TEST_SCENARIO_1,
      processResultsFunc: processResultsGlobal.toString(),
    },
    100,
    runTests
  );
  await setupAndRunCrossOriginIsolatedTest(
    {
      resistFingerprintingPBMOnly: true,
      reduceTimerPrecision: true,
      crossOriginIsolated: true,
      shouldBeRounded: false,
      scenario: TEST_SCENARIO_2,
      processResultsFunc: processResultsGlobal.toString(),
    },
    100,
    runTests
  );
  await setupAndRunCrossOriginIsolatedTest(
    {
      resistFingerprintingPBMOnly: true,
      openPrivateWindow: true,
      reduceTimerPrecision: true,
      crossOriginIsolated: true,
      scenario: TEST_SCENARIO_3,
      processResultsFunc: processResultsGlobal.toString(),
    },
    100,
    runTests
  );

  await setupAndRunCrossOriginIsolatedTest(
    {
      resistFingerprinting: true,
      crossOriginIsolated: true,
      scenario: TEST_SCENARIO_4,
      processResultsFunc: processResultsGlobal.toString(),
    },
    13,
    runTests
  );
  /*
  Disabled because it causes too many intermittents
  await setupAndRunCrossOriginIsolatedTest(
    {
      resistFingerprintingPBMOnly: true,
      crossOriginIsolated: true,
      shouldBeRounded: false,
      scenario: TEST_SCENARIO_5,
      processResultsFunc: processResultsGlobal.toString(),
    },
    13,
    runTests
  );
  */
  await setupAndRunCrossOriginIsolatedTest(
    {
      resistFingerprintingPBMOnly: true,
      openPrivateWindow: true,
      crossOriginIsolated: true,
      scenario: TEST_SCENARIO_6,
      processResultsFunc: processResultsGlobal.toString(),
    },
    13,
    runTests
  );

  // We cannot run the tests with too fine a precision, or it becomes very likely
  // to get false results that a number 'should not been rounded', when it really
  // wasn't, we had just gotten an accidental match
  await setupAndRunCrossOriginIsolatedTest(
    {
      resistFingerprinting: true,
      crossOriginIsolated: true,
      scenario: TEST_SCENARIO_7,
      processResultsFunc: processResultsGlobal.toString(),
    },
    7.97,
    runTests
  );
  /*
  Disabled because it causes too many intermittents
  await setupAndRunCrossOriginIsolatedTest(
    {
      resistFingerprintingPBMOnly: true,
      crossOriginIsolated: true,
      shouldBeRounded: false,
      scenario: TEST_SCENARIO_8,
      processResultsFunc: processResultsGlobal.toString(),
    },
    7.97,
    runTests
  );
  */
  await setupAndRunCrossOriginIsolatedTest(
    {
      resistFingerprintingPBMOnly: true,
      openPrivateWindow: true,
      crossOriginIsolated: true,
      scenario: TEST_SCENARIO_9,
      processResultsFunc: processResultsGlobal.toString(),
    },
    7.97,
    runTests
  );

  // RTP
  await setupAndRunCrossOriginIsolatedTest(
    {
      reduceTimerPrecision: true,
      scenario: TEST_SCENARIO_10,
      processResultsFunc: processResultsGlobal.toString(),
    },
    7.97,
    runTests
  );
  await setupAndRunCrossOriginIsolatedTest(
    {
      reduceTimerPrecision: true,
      crossOriginIsolated: true,
      scenario: TEST_SCENARIO_11,
      processResultsFunc: processResultsGlobal.toString(),
    },
    0.005,
    runTests
  );
});

// ================================================================================================
// ================================================================================================
// This test case is mostly copy-and-paste from the test case for worker in
// test_reduce_time_precision.html. The main difference is this test case
// verifies DOM API has more precsion when it's in cross-origin-isolated and
// cross-origin-isolated doesn't affect RFP.
let runWorkerTest = async function (data) {
  let expectedPrecision = data.precision;
  await new Promise(resolve => {
    // eslint beleives that isrounded is available in this scope, but if you
    // remove the assignment, you will see it is not
    // eslint-disable-next-line
    let isRounded = eval(data.isRoundedFunc);
    // eslint-disable-next-line
    let processResults = eval(data.options.processResultsFunc);

    let worker = new content.Worker(
      "coop_header.sjs?crossOriginIsolated=true&worker=true"
    );

    // Known ways to generate time stamps, in milliseconds
    const timeStampCodes = [
      "new Date().getTime()",
      'new File([], "").lastModified',
      "performance.now()",
      'new Event("").timeStamp',
    ];

    let promises = [],
      successes = [],
      failures = [];
    for (let timeStampCode of timeStampCodes) {
      promises.push(
        new Promise(res => {
          worker.postMessage({
            type: "runCmdAndGetResult",
            cmd: timeStampCode,
          });

          worker.addEventListener("message", function (e) {
            // If we are not rounding values, this function will invert the return value
            let resultSwitchisRounded = function (timeStamp) {
              if (timeStamp == 0) {
                return true;
              }
              let result = isRounded(timeStamp, expectedPrecision);
              return data.options.shouldBeRounded ? result : !result;
            };

            if (e.data.type == "result") {
              if (e.data.resultOf == timeStampCode) {
                if (resultSwitchisRounded(e.data.result)) {
                  successes = successes.concat([
                    [timeStampCode, e.data.result],
                  ]);
                } else {
                  failures = failures.concat([[timeStampCode, e.data.result]]);
                }
                worker.removeEventListener("message", this);
                res();
              }

              return;
            }

            ok(false, `Unknown message type. Got ${e.data.type}`);
            res();
          });
        })
      );
    }

    Promise.all(promises).then(_ => {
      worker.terminate();
      processResults(data, successes, failures);
      resolve();
    });
  });
};

add_task(async function runRTPTestsForWorker() {
  // RFP
  await setupAndRunCrossOriginIsolatedTest(
    {
      resistFingerprinting: true,
      reduceTimerPrecision: true,
      crossOriginIsolated: true,
      scenario: TEST_SCENARIO_101,
      processResultsFunc: processResultsGlobal.toString(),
    },
    100,
    runWorkerTest
  );
  await setupAndRunCrossOriginIsolatedTest(
    {
      resistFingerprintingPBMOnly: true,
      reduceTimerPrecision: true,
      crossOriginIsolated: true,
      shouldBeRounded: false,
      scenario: TEST_SCENARIO_102,
      processResultsFunc: processResultsGlobal.toString(),
    },
    100,
    runWorkerTest
  );
  await setupAndRunCrossOriginIsolatedTest(
    {
      resistFingerprintingPBMOnly: true,
      openPrivateWindow: true,
      reduceTimerPrecision: true,
      crossOriginIsolated: true,
      scenario: TEST_SCENARIO_103,
      processResultsFunc: processResultsGlobal.toString(),
    },
    100,
    runWorkerTest
  );
  await setupAndRunCrossOriginIsolatedTest(
    {
      resistFingerprinting: true,
      crossOriginIsolated: true,
      scenario: TEST_SCENARIO_104,
      processResultsFunc: processResultsGlobal.toString(),
    },
    13,
    runWorkerTest
  );
  /*
  Disabled because it causes too many intermittents
  await setupAndRunCrossOriginIsolatedTest(
    {
      resistFingerprintingPBMOnly: true,
      crossOriginIsolated: true,
      shouldBeRounded: false,
      scenario: TEST_SCENARIO_105,
      processResultsFunc: processResultsGlobal.toString(),
    },
    13,
    runWorkerTest
  );
  */
  await setupAndRunCrossOriginIsolatedTest(
    {
      resistFingerprintingPBMOnly: true,
      openPrivateWindow: true,
      crossOriginIsolated: true,
      scenario: TEST_SCENARIO_106,
      processResultsFunc: processResultsGlobal.toString(),
    },
    13,
    runWorkerTest
  );
  await setupAndRunCrossOriginIsolatedTest(
    {
      resistFingerprinting: true,
      crossOriginIsolated: true,
      scenario: TEST_SCENARIO_107,
      processResultsFunc: processResultsGlobal.toString(),
    },
    7.97,
    runWorkerTest
  );
  /* Disabled because it causes too many intermittents
  await setupAndRunCrossOriginIsolatedTest(
    {
      resistFingerprintingPBMOnly: true,
      crossOriginIsolated: true,
      shouldBeRounded: false,
      scenario: TEST_SCENARIO_108,
      processResultsFunc: processResultsGlobal.toString(),
    },
    7.97,
    runWorkerTest
  );
  */
  await setupAndRunCrossOriginIsolatedTest(
    {
      resistFingerprintingPBMOnly: true,
      openPrivateWindow: true,
      crossOriginIsolated: true,
      scenario: TEST_SCENARIO_109,
      processResultsFunc: processResultsGlobal.toString(),
    },
    7.97,
    runWorkerTest
  );

  // RTP
  await setupAndRunCrossOriginIsolatedTest(
    {
      reduceTimerPrecision: true,
      scenario: TEST_SCENARIO_110,
      processResultsFunc: processResultsGlobal.toString(),
    },
    7.97,
    runWorkerTest
  );
  await setupAndRunCrossOriginIsolatedTest(
    {
      reduceTimerPrecision: true,
      crossOriginIsolated: true,
      scenario: TEST_SCENARIO_111,
      processResultsFunc: processResultsGlobal.toString(),
    },
    0.005,
    runWorkerTest
  );
});
