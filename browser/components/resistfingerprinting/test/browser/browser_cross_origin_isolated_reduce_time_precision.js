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

// ================================================================================================
// ================================================================================================
// This test case is mostly copy-and-paste from the test case for window in
// test_reduce_time_precision.html. The main difference is this test case
// verifies DOM API has more precsion when it's in cross-origin-isolated and
// cross-origin-isolated doesn't affect RFP.
add_task(async function runRTPTestDOM() {
  let runTests = async function(data) {
    let expectedPrecision = data.precision;
    // eslint beleives that isrounded is available in this scope, but if you
    // remove the assignment, you will see it is not
    // eslint-disable-next-line
    let isRounded = eval(data.isRoundedFunc);

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
    let resultSwitchisRounded = function(timeStamp) {
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

    for (let success of successes) {
      ok(
        resultSwitchisRounded(success[1]),
        (data.options.shouldBeRounded ? "Should " : "Should not ") +
          `have rounded '${success[0]}' to nearest ${expectedPrecision} ms; saw ${success[1]}. ` +
          `Scenario: ${data.options.scenario}`
      );
    }
    if (failures.length > 2) {
      for (let failure of failures) {
        ok(
          resultSwitchisRounded(failure[1]),
          (data.options.shouldBeRounded ? "Should " : "Should not ") +
            `have rounded '${failure[0]}' to nearest ${expectedPrecision} ms; saw ${failure[1]}. ` +
            `Scenario: ${data.options.scenario}`
        );
      }
    } else if (
      failures.length == 2 &&
      expectedPrecision < 10 &&
      failures[0][0] == timeStampCodes[0] &&
      failures[1][0] == timeStampCodes[1]
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
          `Scenario: ${data.options.scenario}`
      );
    } else if (failures.length == 1) {
      ok(
        true,
        "Free Failure: " +
          (data.options.shouldBeRounded ? "Should " : "Should not ") +
          `have rounded '${failures[0][0]}' to nearest ${expectedPrecision} ms; saw ${failures[0][1]}. ` +
          `Scenario: ${data.options.scenario}`
      );
    }
  };

  // RFP
  await setupAndRunCrossOriginIsolatedTest(
    {
      resistFingerprinting: true,
      reduceTimerPrecision: true,
      crossOriginIsolated: true,
      scenario: TEST_SCENARIO_1,
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
    },
    100,
    runTests
  );

  await setupAndRunCrossOriginIsolatedTest(
    {
      resistFingerprinting: true,
      crossOriginIsolated: true,
      scenario: TEST_SCENARIO_4,
    },
    13,
    runTests
  );
  await setupAndRunCrossOriginIsolatedTest(
    {
      resistFingerprintingPBMOnly: true,
      crossOriginIsolated: true,
      shouldBeRounded: false,
      scenario: TEST_SCENARIO_5,
    },
    13,
    runTests
  );

  await setupAndRunCrossOriginIsolatedTest(
    {
      resistFingerprintingPBMOnly: true,
      openPrivateWindow: true,
      crossOriginIsolated: true,
      scenario: TEST_SCENARIO_6,
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
    },
    7.97,
    runTests
  );
  await setupAndRunCrossOriginIsolatedTest(
    {
      resistFingerprintingPBMOnly: true,
      crossOriginIsolated: true,
      shouldBeRounded: false,
      scenario: TEST_SCENARIO_8,
    },
    7.97,
    runTests
  );
  await setupAndRunCrossOriginIsolatedTest(
    {
      resistFingerprintingPBMOnly: true,
      openPrivateWindow: true,
      crossOriginIsolated: true,
      scenario: TEST_SCENARIO_9,
    },
    7.97,
    runTests
  );

  // RTP
  await setupAndRunCrossOriginIsolatedTest(
    {
      reduceTimerPrecision: true,
      scenario: TEST_SCENARIO_10,
    },
    7.97,
    runTests
  );
  await setupAndRunCrossOriginIsolatedTest(
    {
      reduceTimerPrecision: true,
      crossOriginIsolated: true,
      scenario: TEST_SCENARIO_11,
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
let runWorkerTest = async function(data) {
  let expectedPrecision = data.precision;
  await new Promise(resolve => {
    // eslint beleives that isrounded is available in this scope, but if you
    // remove the assignment, you will see it is not
    // eslint-disable-next-line
    let isRounded = eval(data.isRoundedFunc);

    let worker = new content.Worker(
      "coop_header.sjs?crossOriginIsolated=true&worker=true"
    );

    // Known ways to generate time stamps, in milliseconds
    const timeStampCodes = [
      "performance.now()",
      "new Date().getTime()",
      'new Event("").timeStamp',
      'new File([], "").lastModified',
    ];

    let promises = [];
    for (let timeStampCode of timeStampCodes) {
      promises.push(
        new Promise(res => {
          worker.postMessage({
            type: "runCmdAndGetResult",
            cmd: timeStampCode,
          });

          worker.addEventListener("message", function(e) {
            // If we are not rounding values, this function will invert the return value
            let resultSwitchisRounded = function(timeStamp) {
              if (timeStamp == 0) {
                return true;
              }
              let result = isRounded(timeStamp, expectedPrecision);
              return data.options.shouldBeRounded ? result : !result;
            };

            if (e.data.type == "result") {
              if (e.data.resultOf == timeStampCode) {
                ok(
                  resultSwitchisRounded(e.data.result),
                  `The result of ${e.data.resultOf} should be rounded to ` +
                    ` nearest ${expectedPrecision} ms in workers; saw ` +
                    `${e.data.result}`
                );
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
    },
    100,
    runWorkerTest
  );
  await setupAndRunCrossOriginIsolatedTest(
    {
      resistFingerprinting: true,
      crossOriginIsolated: true,
    },
    13,
    runWorkerTest
  );
  await setupAndRunCrossOriginIsolatedTest(
    {
      resistFingerprinting: true,
      crossOriginIsolated: true,
    },
    0.13,
    runWorkerTest
  );

  // RTP
  await setupAndRunCrossOriginIsolatedTest(
    {
      reduceTimerPrecision: true,
    },
    0.13,
    runWorkerTest
  );
  await setupAndRunCrossOriginIsolatedTest(
    {
      reduceTimerPrecision: true,
      crossOriginIsolated: true,
    },
    0.005,
    runWorkerTest
  );
});
