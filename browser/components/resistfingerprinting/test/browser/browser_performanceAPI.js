/**
 * Bug 1369303 - A test for making sure that performance APIs have been correctly
 *   spoofed or disabled.
 */

const TEST_PATH =
  "http://example.net/browser/browser/" +
  "components/resistfingerprinting/test/browser/";

const PERFORMANCE_TIMINGS = [
  "navigationStart",
  "unloadEventStart",
  "unloadEventEnd",
  "redirectStart",
  "redirectEnd",
  "fetchStart",
  "domainLookupStart",
  "domainLookupEnd",
  "connectStart",
  "connectEnd",
  "secureConnectionStart",
  "requestStart",
  "responseStart",
  "responseEnd",
  "domLoading",
  "domInteractive",
  "domContentLoadedEventStart",
  "domContentLoadedEventEnd",
  "domComplete",
  "loadEventStart",
  "loadEventEnd",
];

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
    ],
  });

  let url = crossOriginIsolated
    ? `https://example.com/browser/browser/components/resistfingerprinting` +
      `/test/browser/coop_header.sjs?crossOriginIsolated=${crossOriginIsolated}`
    : TEST_PATH + "file_dummy.html";

  let win = await BrowserTestUtils.openNewBrowserWindow();
  let tab = await BrowserTestUtils.openNewForegroundTab(win.gBrowser, url);

  // No matter what we set the precision to, if we're in ResistFingerprinting mode
  // we use the larger of the precision pref and the constant 100ms
  if (resistFingerprinting) {
    expectedPrecision = expectedPrecision < 100 ? 100 : expectedPrecision;
  }
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [
      {
        list: PERFORMANCE_TIMINGS,
        precision: expectedPrecision,
        isRoundedFunc: isTimeValueRounded.toString(),
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
add_task(async function runRPTests() {
  let runTests = async function(data) {
    let timerlist = data.list;
    let expectedPrecision = data.precision;
    // eslint beleives that isrounded is available in this scope, but if you
    // remove the assignment, you will see it is not
    // eslint-disable-next-line
    let isRounded = eval(data.isRoundedFunc);

    ok(
      isRounded(content.performance.timeOrigin, expectedPrecision),
      `For resistFingerprinting, performance.timeOrigin is not correctly rounded: ` +
        content.performance.timeOrigin
    );

    // Check that the performance timing API is correctly spoofed. In
    // particular, check if domainLookupStart and domainLookupEnd return
    // fetchStart, and if everything else is clamped to the expected precision.
    for (let time of timerlist) {
      if (time == "domainLookupStart" || time == "domainLookupEnd") {
        is(
          content.performance.timing[time],
          content.performance.timing.fetchStart,
          `For resistFingerprinting, the timing(${time}) is not correctly spoofed.`
        );
      } else {
        ok(
          isRounded(content.performance.timing[time], expectedPrecision),
          `For resistFingerprinting with expected precision ` +
            expectedPrecision +
            `, the timing(${time}) is not correctly rounded: ` +
            content.performance.timing[time]
        );
      }
    }

    // Try to add some entries.
    content.performance.mark("Test");
    content.performance.mark("Test-End");
    content.performance.measure("Test-Measure", "Test", "Test-End");

    // Check that no entries for performance.getEntries/getEntriesByType/getEntriesByName.
    is(
      content.performance.getEntries().length,
      0,
      "For resistFingerprinting, there should be no entries for performance.getEntries()"
    );
    is(
      content.performance.getEntriesByType("resource").length,
      0,
      "For resistFingerprinting, there should be no entries for performance.getEntriesByType()"
    );
    is(
      content.performance.getEntriesByName("Test", "mark").length,
      0,
      "For resistFingerprinting, there should be no entries for performance.getEntriesByName()"
    );
  };

  await setupTest(true, true, false, 200, runTests);
  await setupTest(true, true, false, 100, runTests);
  await setupTest(true, false, false, 13, runTests);
  await setupTest(true, false, false, 0.13, runTests);
  await setupTest(true, true, true, 100, runTests);
  await setupTest(true, false, true, 13, runTests);
  await setupTest(true, false, true, 0.13, runTests);
});

// ================================================================================================
// ================================================================================================
add_task(async function runRTPTests() {
  let runTests = async function(data) {
    let timerlist = data.list;
    let expectedPrecision = data.precision;
    // eslint beleives that isrounded is available in this scope, but if you
    // remove the assignment, you will see it is not
    // eslint-disable-next-line
    let isRounded = eval(data.isRoundedFunc);

    ok(
      isRounded(content.performance.timeOrigin, expectedPrecision),
      `For reduceTimerPrecision, performance.timeOrigin is not correctly rounded: ` +
        content.performance.timeOrigin
    );

    // Check that whether the performance timing API is correctly spoofed.
    for (let time of timerlist) {
      ok(
        isRounded(content.performance.timing[time], expectedPrecision),
        `For reduceTimerPrecision(` +
          expectedPrecision +
          `), the timing(${time}) is not correctly rounded: ` +
          content.performance.timing[time]
      );
    }

    // Try to add some entries.
    content.performance.mark("Test");
    content.performance.mark("Test-End");
    content.performance.measure("Test-Measure", "Test", "Test-End");

    // Check the entries for performance.getEntries/getEntriesByType/getEntriesByName.
    is(
      content.performance.getEntries().length,
      4,
      "For reduceTimerPrecision, there should be 4 entries for performance.getEntries()"
      // PerformanceNavigationTiming, PerformanceMark, PerformanceMark, PerformanceMeasure
    );
    for (var i = 0; i < 4; i++) {
      let startTime = content.performance.getEntries()[i].startTime;
      let duration = content.performance.getEntries()[i].duration;
      ok(
        isRounded(startTime, expectedPrecision),
        "For reduceTimerPrecision(" +
          expectedPrecision +
          "), performance.getEntries(" +
          i +
          ").startTime is not rounded: " +
          startTime
      );
      ok(
        isRounded(duration, expectedPrecision),
        "For reduceTimerPrecision(" +
          expectedPrecision +
          "), performance.getEntries(" +
          i +
          ").duration is not rounded: " +
          duration
      );
    }
    is(
      content.performance.getEntriesByType("mark").length,
      2,
      "For reduceTimerPrecision, there should be 2 entries for performance.getEntriesByType()"
    );
    is(
      content.performance.getEntriesByName("Test", "mark").length,
      1,
      "For reduceTimerPrecision, there should be 1 entry for performance.getEntriesByName()"
    );
    content.performance.clearMarks();
    content.performance.clearMeasures();
    content.performance.clearResourceTimings();
  };

  await setupTest(false, true, false, 100, runTests);
  await setupTest(false, true, false, 13, runTests);
  await setupTest(false, true, false, 0.13, runTests);
  await setupTest(false, true, true, 0.005, runTests);
});

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
  await setupTest(true, true, false, 100, runWorkerTest, "runRPTests");
  await setupTest(true, false, false, 13, runWorkerTest, "runRPTests");
  await setupTest(true, true, false, 0.13, runWorkerTest, "runRPTests");
});

add_task(async function runRTPTestsForWorker() {
  await setupTest(false, true, false, 100, runWorkerTest, "runRTPTests");
  await setupTest(false, true, false, 13, runWorkerTest, "runRTPTests");
  await setupTest(false, true, false, 0.13, runWorkerTest, "runRTPTests");
});
