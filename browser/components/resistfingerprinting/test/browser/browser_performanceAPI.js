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

add_task(async function runRPTests() {
  await SpecialPowers.pushPrefEnv({"set":
    //Run one set of tests with both true to confirm p.rP overrides p.rTP
    [["privacy.resistFingerprinting", true],
     ["privacy.reduceTimerPrecision", true]]
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser, TEST_PATH + "file_dummy.html");

  await ContentTask.spawn(tab.linkedBrowser, PERFORMANCE_TIMINGS, async function(list) {
    const isRounded = x => (Math.floor(x / 100) * 100) === x;

    ok(isRounded(content.performance.timeOrigin), `For resistFingerprinting, performance.timeOrigin is not correctly rounded: ` + content.performance.timeOrigin);

    // Check that whether the performance timing API is correctly spoofed.
    for (let time of list) {
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

  });

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function runRPTestsForWorker() {
  await SpecialPowers.pushPrefEnv({"set":
    [["privacy.resistFingerprinting", true],
     ["privacy.reduceTimerPrecision", false]]
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser, TEST_PATH + "file_dummy.html");

  await ContentTask.spawn(tab.linkedBrowser, null, async function() {
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
      worker.postMessage({type: "runRPTests"});
    });
  });

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function runRTPTests() {
  await SpecialPowers.pushPrefEnv({"set":
    [["privacy.resistFingerprinting", false],
     ["privacy.reduceTimerPrecision", true]]
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser, TEST_PATH + "file_dummy.html");

  await ContentTask.spawn(tab.linkedBrowser, PERFORMANCE_TIMINGS, async function(list) {
    const isRounded = x => (Math.floor(x / 100) * 100) === x;

    ok(isRounded(content.performance.timeOrigin), `For reduceTimerPrecision, performance.timeOrigin is not correctly rounded: ` + content.performance.timeOrigin);

    // Check that whether the performance timing API is correctly spoofed.
    for (let time of list) {
      ok(isRounded(content.performance.timing[time]), `For reduceTimerPrecision, the timing(${time}) is not correctly rounded.`);
    }

    // Try to add some entries.
    content.performance.mark("Test");
    content.performance.mark("Test-End");
    content.performance.measure("Test-Measure", "Test", "Test-End");

    // Check that no entries for performance.getEntries/getEntriesByType/getEntriesByName.
    is(content.performance.getEntries().length, 4, "For reduceTimerPrecision, there should be 4 entries for performance.getEntries()");
    for(var i=0; i<4; i++) {
      let startTime = content.performance.getEntries()[i].startTime;
      let duration = content.performance.getEntries()[i].duration;
      ok(isRounded(startTime), "For reduceTimerPrecision, performance.getEntries(" + i.toString() + ").startTime is not rounded: " + startTime.toString());
      ok(isRounded(duration), "For reduceTimerPrecision, performance.getEntries(" + i.toString() + ").duration is not rounded: " + duration.toString());
    }
    is(content.performance.getEntriesByType("mark").length, 2, "For reduceTimerPrecision, there should be 2 entries for performance.getEntriesByType()");
    is(content.performance.getEntriesByName("Test", "mark").length, 1, "For reduceTimerPrecision, there should be 1 entry for performance.getEntriesByName()");

  });

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function runRTPTestsForWorker() {
  await SpecialPowers.pushPrefEnv({"set":
    [["privacy.resistFingerprinting", false],
     ["privacy.reduceTimerPrecision", true]]
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser, TEST_PATH + "file_dummy.html");

  await ContentTask.spawn(tab.linkedBrowser, null, async function() {
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
      worker.postMessage({type: "runRTPTests"});
    });
  });

  await BrowserTestUtils.removeTab(tab);
});