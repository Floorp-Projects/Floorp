/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const ROOT_URL = "http://example.com/browser/dom/tests/browser/perfmetrics";
const DUMMY_URL = ROOT_URL + "/dummy.html";
const WORKER_URL = ROOT_URL + "/ping_worker.html";
const WORKER_URL2 = ROOT_URL + "/ping_worker2.html";
const INTERVAL_URL = ROOT_URL + "/setinterval.html";
const TIMEOUT_URL = ROOT_URL + "/settimeout.html";
const SOUND_URL = ROOT_URL + "/sound.html";
const CATEGORY_TIMER = 2;

let nextId = 0;

function jsonrpc(tab, method, params) {
  let currentId = nextId++;
  let messageManager = tab.linkedBrowser.messageManager;
  messageManager.sendAsyncMessage("jsonrpc", {
    id: currentId,
    method,
    params,
  });
  return new Promise(function(resolve, reject) {
    messageManager.addMessageListener("jsonrpc", function listener(event) {
      let { id, result, error } = event.data;
      if (id !== currentId) {
        return;
      }
      messageManager.removeMessageListener("jsonrpc", listener);
      if (error) {
        reject(error);
        return;
      }
      resolve(result);
    });
  });
}

function postMessageToWorker(tab, message) {
  return jsonrpc(tab, "postMessageToWorker", [WORKER_URL, message]);
}

add_task(async function test() {
  waitForExplicitFinish();

  // Load 3 pages and wait. The 3rd one has a worker
  let page1 = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "about:about",
    forceNewProcess: false,
  });

  let page2 = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "about:memory",
    forceNewProcess: false,
  });

  let page3 = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: WORKER_URL,
  });
  // load a 4th tab with a worker
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: WORKER_URL2 },
    async function(browser) {
      // grab events..
      let workerDuration = 0;
      let workerTotal = 0;
      let duration = 0;
      let total = 0;
      let isTopLevel = false;
      let aboutMemoryFound = false;
      let parentProcessEvent = false;
      let subFrameIds = [];
      let topLevelIds = [];
      let sharedWorker = false;
      let counterIds = [];
      let timerCalls = 0;
      let heapUsage = 0;
      let mediaMemory = 0;

      function exploreResults(data, filterByWindowId) {
        for (let entry of data) {
          if (filterByWindowId && entry.windowId != filterByWindowId) {
            continue;
          }
          if (!counterIds.includes(entry.pid + ":" + entry.counterId)) {
            counterIds.push(entry.pid + ":" + entry.counterId);
          }
          sharedWorker =
            entry.host.endsWith("shared_worker.js") || sharedWorker;
          heapUsage += entry.memoryInfo.GCHeapUsage;
          mediaMemory +=
            entry.memoryInfo.media.audioSize +
            entry.memoryInfo.media.resourcesSize;
          Assert.ok(
            entry.host != "" || entry.windowId != 0,
            "An entry should have a host or a windowId"
          );
          if (
            entry.windowId != 0 &&
            !entry.isToplevel &&
            !entry.isWorker &&
            !subFrameIds.includes(entry.windowId)
          ) {
            subFrameIds.push(entry.windowId);
          }
          if (entry.isTopLevel && !topLevelIds.includes(entry.windowId)) {
            topLevelIds.push(entry.windowId);
          }
          if (entry.host == "example.com" && entry.isTopLevel) {
            isTopLevel = true;
          }
          if (entry.host == "about:memory") {
            aboutMemoryFound = true;
          }
          if (entry.pid == Services.appinfo.processID) {
            parentProcessEvent = true;
          }
          if (entry.isWorker) {
            workerDuration += entry.duration;
          } else {
            duration += entry.duration;
          }
          // let's look at the data we got back
          for (let item of entry.items) {
            Assert.ok(
              item.count > 0,
              "Categories with an empty count are dropped"
            );
            if (entry.isWorker) {
              workerTotal += item.count;
            } else {
              total += item.count;
            }
            if (item.category == CATEGORY_TIMER) {
              timerCalls += item.count;
            }
          }
        }
      }

      // get all metrics via the promise
      let results = await ChromeUtils.requestPerformanceMetrics();
      exploreResults(results);

      Assert.ok(workerDuration > 0, "Worker duration should be positive");
      Assert.ok(workerTotal > 0, "Worker count should be positive");
      Assert.ok(duration > 0, "Duration should be positive");
      Assert.ok(total > 0, "Should get a positive count");
      Assert.ok(parentProcessEvent, "parent process sent back some events");
      Assert.ok(isTopLevel, "example.com as a top level window");
      Assert.ok(aboutMemoryFound, "about:memory");
      Assert.ok(heapUsage > 0, "got some memory value reported");
      Assert.ok(sharedWorker, "We got some info from a shared worker");
      let numCounters = counterIds.length;
      Assert.ok(
        numCounters > 5,
        "This test generated at least " + numCounters + " unique counters"
      );

      // checking that subframes are not orphans
      for (let frameId of subFrameIds) {
        Assert.ok(topLevelIds.includes(frameId), "subframe is not orphan ");
      }

      // Doing a second call, we shoud get bigger values
      let previousWorkerDuration = workerDuration;
      let previousWorkerTotal = workerTotal;
      let previousDuration = duration;
      let previousTotal = total;

      results = await ChromeUtils.requestPerformanceMetrics();
      exploreResults(results);

      Assert.ok(
        workerDuration > previousWorkerDuration,
        "Worker duration should be positive"
      );
      Assert.ok(
        workerTotal > previousWorkerTotal,
        "Worker count should be positive"
      );
      Assert.ok(duration > previousDuration, "Duration should be positive");
      Assert.ok(total > previousTotal, "Should get a positive count");

      // load a tab with a setInterval, we should get counters on TaskCategory::Timer
      await BrowserTestUtils.withNewTab(
        { gBrowser, url: INTERVAL_URL },
        async function(browser) {
          let tabId = gBrowser.selectedBrowser.outerWindowID;
          let previousTimerCalls = timerCalls;
          results = await ChromeUtils.requestPerformanceMetrics();
          exploreResults(results, tabId);
          Assert.ok(timerCalls > previousTimerCalls, "Got timer calls");
        }
      );

      // load a tab with a setTimeout, we should get counters on TaskCategory::Timer
      await BrowserTestUtils.withNewTab(
        { gBrowser, url: TIMEOUT_URL },
        async function(browser) {
          let tabId = gBrowser.selectedBrowser.outerWindowID;
          let previousTimerCalls = timerCalls;
          results = await ChromeUtils.requestPerformanceMetrics();
          exploreResults(results, tabId);
          Assert.ok(timerCalls > previousTimerCalls, "Got timer calls");
        }
      );

      // load a tab with a sound
      await BrowserTestUtils.withNewTab(
        { gBrowser, url: SOUND_URL },
        async function(browser) {
          let tabId = gBrowser.selectedBrowser.outerWindowID;
          results = await ChromeUtils.requestPerformanceMetrics();
          exploreResults(results, tabId);
          Assert.ok(mediaMemory > 0, "Got some memory used for media");
        }
      );
    }
  );

  BrowserTestUtils.removeTab(page1);
  BrowserTestUtils.removeTab(page2);
  BrowserTestUtils.removeTab(page3);
});
