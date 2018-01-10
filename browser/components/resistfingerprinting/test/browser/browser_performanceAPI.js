/**
 * Bug 1369303 - A test for making sure that performance APIs have been correctly
 *   spoofed or disabled.
 */

const TEST_PATH = "http://example.net/browser/browser/" +
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

let isRounded = (x, expectedPrecision) => {
  let rounded = (Math.floor(x / expectedPrecision) * expectedPrecision);
  // First we do the perfectly normal check that should work just fine
  if (rounded === x || x === 0)
    return true;

  // When we're diving by non-whole numbers, we may not get perfect
  // multiplication/division because of floating points
  if (Math.abs(rounded - x + expectedPrecision) < .0000001) {
    return true;
  } else if (Math.abs(rounded - x) < .0000001) {
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

  ok(false, "Looming Test Failure, Additional Debugging Info: Expected Precision: " + expectedPrecision + " Measured Value: " + x +
    " Rounded Vaue: " + rounded + " Fuzzy1: " + Math.abs(rounded - x + expectedPrecision) +
    " Fuzzy 2: " + Math.abs(rounded - x));

  return false;
};

add_task(async function runRPTests() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser, TEST_PATH + "file_dummy.html");

  let runTests = async function(data) {
    let timerlist = data.list;
    let expectedPrecision = data.precision;
    // eslint beleives that isrounded is available in this scope, but if you
    // remove the assignment, you will see it is not
    // eslint-disable-next-line
    let isRounded = eval(data.isRoundedFunc);

    ok(isRounded(content.performance.timeOrigin, expectedPrecision), `For resistFingerprinting, performance.timeOrigin is not correctly rounded: ` + content.performance.timeOrigin);

    // Check that whether the performance timing API is correctly spoofed.
    for (let time of timerlist) {
      is(content.performance.timing[time], 0, `For resistFingerprinting, the timing(${time}) is not correctly spoofed.`);
    }

    // Try to add some entries.
    content.performance.mark("Test");
    content.performance.mark("Test-End");
    content.performance.measure("Test-Measure", "Test", "Test-End");

    // Check that no entries for performance.getEntries/getEntriesByType/getEntriesByName.
    is(content.performance.getEntries().length, 0, "For resistFingerprinting, there should be no entries for performance.getEntries()");
    is(content.performance.getEntriesByType("resource").length, 0, "For resistFingerprinting, there should be no entries for performance.getEntriesByType()");
    is(content.performance.getEntriesByName("Test", "mark").length, 0, "For resistFingerprinting, there should be no entries for performance.getEntriesByName()");

  };

  let expectedPrecision = 100;
  await SpecialPowers.pushPrefEnv({"set":
    // Run one set of tests with both true to confirm p.rP overrides p.rTP
    [["privacy.resistFingerprinting", true],
     ["privacy.reduceTimerPrecision", true],
     ["privacy.resistFingerprinting.reduceTimerPrecision.microseconds", expectedPrecision * 1000]
     ]
  });
  await ContentTask.spawn(tab.linkedBrowser, { list: PERFORMANCE_TIMINGS, precision: expectedPrecision, isRoundedFunc: isRounded.toString() }, runTests);

  expectedPrecision = 13;
  await SpecialPowers.pushPrefEnv({"set":
    // Run one set of tests with both true to confirm p.rP overrides p.rTP
    [["privacy.resistFingerprinting", true],
     ["privacy.reduceTimerPrecision", true],
     ["privacy.resistFingerprinting.reduceTimerPrecision.microseconds", expectedPrecision * 1000]
     ]
  });
  await ContentTask.spawn(tab.linkedBrowser, { list: PERFORMANCE_TIMINGS, precision: expectedPrecision, isRoundedFunc: isRounded.toString() }, runTests);

  expectedPrecision = .13;
  await SpecialPowers.pushPrefEnv({"set":
    // Run one set of tests with both true to confirm p.rP overrides p.rTP
    [["privacy.resistFingerprinting", true],
     ["privacy.reduceTimerPrecision", true],
     ["privacy.resistFingerprinting.reduceTimerPrecision.microseconds", expectedPrecision * 1000]
     ]
  });
  await ContentTask.spawn(tab.linkedBrowser, { list: PERFORMANCE_TIMINGS, precision: expectedPrecision, isRoundedFunc: isRounded.toString() }, runTests);

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function runRPTestsForWorker() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser, TEST_PATH + "file_dummy.html");

  let runTest = async function(expectedPrecision) {
    await new Promise(resolve => {
      let worker = new content.Worker("file_workerPerformance.js");
      worker.onmessage = function(e) {
        if (e.data.type == "status") {
          ok(e.data.status, e.data.msg);
        } else if (e.data.type == "finish") {
          resolve();
        } else {
          ok(false, "Unknown message type");
          resolve();
        }
      };
      worker.postMessage({type: "runRPTests", precision: expectedPrecision});
    });
  };

  let expectedPrecision = 100;
    // Run one set of tests with both true to confirm p.rP overrides p.rTP
  await SpecialPowers.pushPrefEnv({"set":
    [["privacy.resistFingerprinting", true],
     ["privacy.reduceTimerPrecision", true],
     ["privacy.resistFingerprinting.reduceTimerPrecision.microseconds", expectedPrecision * 1000]]
  });
  await ContentTask.spawn(tab.linkedBrowser, expectedPrecision, runTest);

  expectedPrecision = 13;
  await SpecialPowers.pushPrefEnv({"set":
    [["privacy.resistFingerprinting", true],
     ["privacy.reduceTimerPrecision", false],
     ["privacy.resistFingerprinting.reduceTimerPrecision.microseconds", expectedPrecision * 1000]]
  });
  await ContentTask.spawn(tab.linkedBrowser, expectedPrecision, runTest);

  expectedPrecision = .13;
  await SpecialPowers.pushPrefEnv({"set":
    [["privacy.resistFingerprinting", true],
     ["privacy.reduceTimerPrecision", false],
     ["privacy.resistFingerprinting.reduceTimerPrecision.microseconds", expectedPrecision * 1000]]
  });
  await ContentTask.spawn(tab.linkedBrowser, expectedPrecision, runTest);

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function runRTPTests() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser, TEST_PATH + "file_dummy.html");

  let runTests = async function(data) {
    let timerlist = data.list;
    let expectedPrecision = data.precision;
    // eslint beleives that isrounded is available in this scope, but if you
    // remove the assignment, you will see it is not
    // eslint-disable-next-line
    let isRounded = eval(data.isRoundedFunc);

    ok(isRounded(content.performance.timeOrigin, expectedPrecision), `For reduceTimerPrecision, performance.timeOrigin is not correctly rounded: ` + content.performance.timeOrigin);

    // Check that whether the performance timing API is correctly spoofed.
    for (let time of timerlist) {
      ok(isRounded(content.performance.timing[time], expectedPrecision), `For reduceTimerPrecision(` + expectedPrecision + `), the timing(${time}) is not correctly rounded: ` + content.performance.timing[time]);
    }

    // Try to add some entries.
    content.performance.mark("Test");
    content.performance.mark("Test-End");
    content.performance.measure("Test-Measure", "Test", "Test-End");

    // Check the entries for performance.getEntries/getEntriesByType/getEntriesByName.
    is(content.performance.getEntries().length, 4, "For reduceTimerPrecision, there should be 4 entries for performance.getEntries()");
    for (var i = 0; i < 4; i++) {
      let startTime = content.performance.getEntries()[i].startTime;
      let duration = content.performance.getEntries()[i].duration;
      ok(isRounded(startTime, expectedPrecision), "For reduceTimerPrecision(" + expectedPrecision + "), performance.getEntries(" + i + ").startTime is not rounded: " + startTime);
      ok(isRounded(duration, expectedPrecision), "For reduceTimerPrecision(" + expectedPrecision + "), performance.getEntries(" + i + ").duration is not rounded: " + duration);
    }
    is(content.performance.getEntriesByType("mark").length, 2, "For reduceTimerPrecision, there should be 2 entries for performance.getEntriesByType()");
    is(content.performance.getEntriesByName("Test", "mark").length, 1, "For reduceTimerPrecision, there should be 1 entry for performance.getEntriesByName()");
    content.performance.clearMarks();
    content.performance.clearMeasures();
    content.performance.clearResourceTimings();
  };

  let expectedPrecision = 100;
  await SpecialPowers.pushPrefEnv({"set":
    [["privacy.resistFingerprinting", false],
     ["privacy.reduceTimerPrecision", true],
     ["privacy.resistFingerprinting.reduceTimerPrecision.microseconds", expectedPrecision * 1000]
    ]
    });
  await ContentTask.spawn(tab.linkedBrowser, { list: PERFORMANCE_TIMINGS, precision: expectedPrecision, isRoundedFunc: isRounded.toString() }, runTests);

  expectedPrecision = 13;
  await SpecialPowers.pushPrefEnv({"set":
    [["privacy.resistFingerprinting", false],
     ["privacy.reduceTimerPrecision", true],
     ["privacy.resistFingerprinting.reduceTimerPrecision.microseconds", expectedPrecision * 1000]
    ]
  });
  await ContentTask.spawn(tab.linkedBrowser, { list: PERFORMANCE_TIMINGS, precision: expectedPrecision, isRoundedFunc: isRounded.toString() }, runTests);

  expectedPrecision = .13;
  await SpecialPowers.pushPrefEnv({"set":
    [["privacy.resistFingerprinting", false],
     ["privacy.reduceTimerPrecision", true],
     ["privacy.resistFingerprinting.reduceTimerPrecision.microseconds", expectedPrecision * 1000]
    ]
  });
  await ContentTask.spawn(tab.linkedBrowser, { list: PERFORMANCE_TIMINGS, precision: expectedPrecision, isRoundedFunc: isRounded.toString() }, runTests);

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function runRTPTestsForWorker() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser, TEST_PATH + "file_dummy.html");

  let runTest = async function(expectedPrecision) {
    await new Promise(resolve => {
      let worker = new content.Worker("file_workerPerformance.js");
      worker.onmessage = function(e) {
        if (e.data.type == "status") {
          ok(e.data.status, e.data.msg);
        } else if (e.data.type == "finish") {
          resolve();
        } else {
          ok(false, "Unknown message type");
          resolve();
        }
      };
      worker.postMessage({type: "runRTPTests", precision: expectedPrecision});
    });
  };

  let expectedPrecision = 100;
  await SpecialPowers.pushPrefEnv({"set":
    [["privacy.resistFingerprinting", false],
     ["privacy.reduceTimerPrecision", true],
     ["privacy.resistFingerprinting.reduceTimerPrecision.microseconds", expectedPrecision * 1000]]
  });
  await ContentTask.spawn(tab.linkedBrowser, expectedPrecision, runTest);

  expectedPrecision = 13;
  await SpecialPowers.pushPrefEnv({"set":
    [["privacy.resistFingerprinting", false],
     ["privacy.reduceTimerPrecision", true],
     ["privacy.resistFingerprinting.reduceTimerPrecision.microseconds", expectedPrecision * 1000]]
  });
  await ContentTask.spawn(tab.linkedBrowser, expectedPrecision, runTest);

  expectedPrecision = .13;
  await SpecialPowers.pushPrefEnv({"set":
    [["privacy.resistFingerprinting", false],
     ["privacy.reduceTimerPrecision", true],
     ["privacy.resistFingerprinting.reduceTimerPrecision.microseconds", expectedPrecision * 1000]]
  });
  await ContentTask.spawn(tab.linkedBrowser, expectedPrecision, runTest);
  await BrowserTestUtils.removeTab(tab);
});
