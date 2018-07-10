/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const ROOT_URL = "http://example.com/browser/dom/tests/browser";
const DUMMY_URL = ROOT_URL + "/dummy.html";
const WORKER_URL = ROOT_URL + "/ping_worker.html";


let nextId = 0;

function jsonrpc(tab, method, params) {
  let currentId = nextId++;
  let messageManager = tab.linkedBrowser.messageManager;
  messageManager.sendAsyncMessage("jsonrpc", {
    id: currentId,
    method: method,
    params: params
  });
  return new Promise(function (resolve, reject) {
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
  SpecialPowers.setBoolPref('dom.performance.enable_scheduler_timing', true);
  waitForExplicitFinish();

  // Load 3 pages and wait. The 3rd one has a worker
  let page1 = await BrowserTestUtils.openNewForegroundTab({
    gBrowser, opening: 'about:about', forceNewProcess: false
  });

  let page2 = await BrowserTestUtils.openNewForegroundTab({
    gBrowser, opening: 'about:memory', forceNewProcess: false
  });

  let page3 = await BrowserTestUtils.openNewForegroundTab({
    gBrowser, opening: "about:performance", forceNewProcess: true
  });

  // load a 4th tab with a worker
  await BrowserTestUtils.withNewTab({ gBrowser, url: WORKER_URL },
    async function(browser) {
    // grab events..
    let workerDuration = 0;
    let workerTotal = 0;
    let duration = 0;
    let total = 0;
    let isTopLevel = false;
    let aboutMemoryFound = false;
    let parentProcessEvent = false;
    let workerEvent = false;

    function exploreResults(data) {
      for (let entry of data) {
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
          workerEvent = true;
          workerDuration += entry.duration;
        } else {
          duration += entry.duration;
        }
        // let's look at the data we got back
        for (let item of entry.items) {
          if (entry.isWorker) {
            workerTotal += item.count;
          } else {
            total += item.count;
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
  });

  BrowserTestUtils.removeTab(page1);
  BrowserTestUtils.removeTab(page2);
  BrowserTestUtils.removeTab(page3);
  SpecialPowers.clearUserPref('dom.performance.enable_scheduler_timing');
});
