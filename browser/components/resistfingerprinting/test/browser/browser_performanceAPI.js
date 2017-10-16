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

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({"set":
    [["privacy.resistFingerprinting", true]]
  });
});

add_task(async function runTests() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser, TEST_PATH + "file_dummy.html");

  await ContentTask.spawn(tab.linkedBrowser, PERFORMANCE_TIMINGS, async function(list) {
    // Check that whether the performance timing API is correctly spoofed.
    for (let time of list) {
      is(content.performance.timing[time], 0, `The timing(${time}) is correctly spoofed.`);
    }

    // Try to add some entries.
    content.performance.mark("Test");
    content.performance.mark("Test-End");
    content.performance.measure("Test-Measure", "Test", "Test-End");

    // Check that no entries for performance.getEntries/getEntriesByType/getEntriesByName.
    is(content.performance.getEntries().length, 0, "No entries for performance.getEntries()");
    is(content.performance.getEntriesByType("resource").length, 0, "No entries for performance.getEntriesByType()");
    is(content.performance.getEntriesByName("Test", "mark").length, 0, "No entries for performance.getEntriesByName()");

  });

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function runTestsForWorker() {
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
      worker.postMessage({type: "runTests"});
    });
  });

  await BrowserTestUtils.removeTab(tab);
});
