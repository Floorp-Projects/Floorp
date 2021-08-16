/**
 * Bug 1621677 - A test for making sure getting the correct (higher) precision
 *   when it's cross-origin-isolated.
 */

// ================================================================================================
// ================================================================================================
// This test case is mostly copy-and-paste from the test case for window in
// test_reduce_time_precision.html. The main difference is this test case
// verifies DOM API has more precsion when it's in cross-origin-isolated and
// cross-origin-isolated doesn't affect RFP.
add_task(async function runRTPTestDOM() {
  let runTests = async function(data) {
    let expectedPrecision = data.precision;
    var isRounded = function() {
      return "Placeholder for eslint";
    };
    let evalStr = "var isRounded = " + data.isRoundedFunc;
    // eslint-disable-next-line
    eval(evalStr);

    // Prepare for test of AudioContext.currentTime
    // eslint-disable-next-line
    let audioContext = new content.AudioContext();

    // Known ways to generate time stamps, in milliseconds
    const timeStampCodes = [
      "content.performance.now()",
      "new content.Date().getTime()",
      'new content.Event("").timeStamp',
      'new content.File([], "").lastModified',
    ];
    // These are measured in seconds, so we need to scale them up
    var timeStampCodesDOM = timeStampCodes.concat([
      "audioContext.currentTime * 1000",
    ]);

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

      ok(
        isRounded(timeStamp, expectedPrecision),
        `Should be rounded to nearest ${expectedPrecision} ms; saw ${timeStamp}`
      );
    }
  };

  // RFP
  await setupAndRunCrossOriginIsolatedTest(true, true, true, 100, runTests);
  await setupAndRunCrossOriginIsolatedTest(true, false, true, 13, runTests);
  await setupAndRunCrossOriginIsolatedTest(true, false, true, 0.13, runTests);

  // RTP
  await setupAndRunCrossOriginIsolatedTest(false, true, false, 0.13, runTests);
  await setupAndRunCrossOriginIsolatedTest(false, true, true, 0.005, runTests);
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
    var isRounded = function() {
      return "Placeholder for eslint";
    };
    let evalStr = "var isRounded = " + data.isRoundedFunc;
    // eslint-disable-next-line
    eval(evalStr);

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
            if (e.data.type == "result") {
              if (e.data.resultOf == timeStampCode) {
                ok(
                  isRounded(e.data.result, expectedPrecision),
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
    true,
    true,
    true,
    100,
    runWorkerTest
  );
  await setupAndRunCrossOriginIsolatedTest(
    true,
    false,
    true,
    13,
    runWorkerTest
  );
  await setupAndRunCrossOriginIsolatedTest(
    true,
    false,
    true,
    0.13,
    runWorkerTest
  );

  // RTP
  await setupAndRunCrossOriginIsolatedTest(
    false,
    true,
    false,
    0.13,
    runWorkerTest
  );
  await setupAndRunCrossOriginIsolatedTest(
    false,
    true,
    true,
    0.005,
    runWorkerTest
  );
});
