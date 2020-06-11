/**
 * Bug 1621677 - A test for making sure getting the correct (higher) precision
 *   when it's cross-origin-isolated.
 */

let isRounded = (x, expectedPrecision) => {
  let rounded = Math.floor(x / expectedPrecision) * expectedPrecision;
  // First we do the perfectly normal check that should work just fine
  if (rounded === x || x === 0) {
    return true;
  }

  // When we're diving by non-whole numbers, we may not get perfect
  // multiplication/division because of floating points.
  // When dealing with ms since epoch, a double's precision is on the order
  // of 1/5 of a microsecond, so we use a value a little higher than that as
  // our epsilon.
  // To be clear, this error is introduced in our re-calculation of 'rounded'
  // above in JavaScript.
  if (Math.abs(rounded - x + expectedPrecision) < 0.0005) {
    return true;
  } else if (Math.abs(rounded - x) < 0.0005) {
    return true;
  }

  // Then we handle the case where you're sub-millisecond and the timer is not
  // We check that the timer is not sub-millisecond by assuming it is not if it
  // returns an even number of milliseconds
  if (expectedPrecision < 1 && Math.round(x) == x) {
    if (Math.round(rounded) == x) {
      return true;
    }
  }

  ok(
    false,
    "Looming Test Failure, Additional Debugging Info: Expected Precision: " +
      expectedPrecision +
      " Measured Value: " +
      x +
      " Rounded Vaue: " +
      rounded +
      " Fuzzy1: " +
      Math.abs(rounded - x + expectedPrecision) +
      " Fuzzy 2: " +
      Math.abs(rounded - x)
  );

  return false;
};

let setupTest = async function(
  resistFingerprinting,
  reduceTimerPrecision,
  crossOriginIsolated,
  expectedPrecision,
  runTests,
  workerCall
) {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.resistFingerprinting", resistFingerprinting],
      ["privacy.reduceTimerPrecision", reduceTimerPrecision],
      [
        "privacy.resistFingerprinting.reduceTimerPrecision.microseconds",
        expectedPrecision * 1000,
      ],
      ["browser.tabs.remote.useCrossOriginOpenerPolicy", crossOriginIsolated],
      ["browser.tabs.remote.useCrossOriginEmbedderPolicy", crossOriginIsolated],
      ["browser.tabs.documentchannel", crossOriginIsolated],
    ],
  });

  let win = await BrowserTestUtils.openNewBrowserWindow();
  let tab = await BrowserTestUtils.openNewForegroundTab(
    win.gBrowser,
    `https://example.com/browser/browser/components/resistfingerprinting` +
      `/test/browser/coop_header.sjs?crossOriginIsolated=${crossOriginIsolated}`
  );

  // No matter what we set the precision to, if we're in ResistFingerprinting mode
  // we use the larger of the precision pref and the constant 100ms
  if (resistFingerprinting) {
    expectedPrecision = expectedPrecision < 100 ? 100 : expectedPrecision;
  }
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [
      {
        precision: expectedPrecision,
        isRoundedFunc: isRounded.toString(),
        workerCall,
      },
    ],
    runTests
  );

  if (crossOriginIsolated) {
    let remoteType = tab.linkedBrowser.remoteType;
    ok(
      remoteType.startsWith(E10SUtils.WEB_REMOTE_COOP_COEP_TYPE_PREFIX),
      `${remoteType} expected to be coop+coep`
    );
  }

  await BrowserTestUtils.closeWindow(win);
};

// ================================================================================================
// ================================================================================================
// This test case is mostly copy-and-paste from the forth test case in
// browser_performanceAPI.js. The main difference is this test case verifies
// performance API have more precsion when it's in cross-origin-isolated and
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
  await setupTest(true, true, true, 100, runWorkerTest, "runRPTests");
  await setupTest(true, false, true, 13, runWorkerTest, "runRPTests");
  await setupTest(true, true, true, 0.13, runWorkerTest, "runRPTests");

  // RTP
  await setupTest(false, true, false, 0.13, runWorkerTest, "runRTPTests");
  await setupTest(false, true, true, 0.005, runWorkerTest, "runRTPTests");
});

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
  await setupTest(true, true, true, 100, runTests);
  await setupTest(true, false, true, 13, runTests);
  await setupTest(true, false, true, 0.13, runTests);

  // RTP
  await setupTest(false, true, false, 0.13, runTests);
  await setupTest(false, true, true, 0.005, runTests);
});

// ================================================================================================
// ================================================================================================
// This test case is mostly copy-and-paste from the test case for worker in
// test_reduce_time_precision.html. The main difference is this test case
// verifies DOM API has more precsion when it's in cross-origin-isolated and
// cross-origin-isolated doesn't affect RFP.
let runWorkerTest1 = async function(data) {
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
  await setupTest(true, true, true, 100, runWorkerTest1);
  await setupTest(true, false, true, 13, runWorkerTest1);
  await setupTest(true, false, true, 0.13, runWorkerTest1);

  // RTP
  await setupTest(false, true, false, 0.13, runWorkerTest1);
  await setupTest(false, true, true, 0.005, runWorkerTest1);
});
