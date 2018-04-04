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

  let parent_process_event = false;
  let worker_event = false;

  // load a 4th tab with a worker
  await BrowserTestUtils.withNewTab({ gBrowser, url: WORKER_URL },
    async function(browser) {
    // grab events..
    var events = [];
    function getInfoFromService(subject, topic, value) {
      subject = subject.QueryInterface(Ci.nsIMutableArray);
      let enumerator = subject.enumerate();
      while (enumerator.hasMoreElements()) {
        let item = enumerator.getNext();
        item = item.QueryInterface(Ci.nsIPerformanceMetricsData);
        if (item.pid == Services.appinfo.processID) {
          parent_process_event = true;
        }
        if (item.worker) {
          worker_event = true;
        }
        events.push(item);
      }
    }

    Services.obs.addObserver(getInfoFromService, "performance-metrics");

    // wait until we get some events back by triggering requestPerformanceMetrics
    await BrowserTestUtils.waitForCondition(() => {
      ChromeUtils.requestPerformanceMetrics();
      return events.length > 10;
    }, "wait for events to come in", 500, 10);

    BrowserTestUtils.removeTab(page1);
    BrowserTestUtils.removeTab(page2);
    BrowserTestUtils.removeTab(page3);

    // let's check the events
    let duration = 0;
    let total = 0;
    for (let i=0; i < events.length; i++) {
      duration += events[i].duration;
      // let's look at the XPCOM data we got back
      let items = events[i].items.QueryInterface(Ci.nsIMutableArray);
      let enumerator = items.enumerate();
      while (enumerator.hasMoreElements()) {
        let item = enumerator.getNext();
        item = item.QueryInterface(Ci.nsIPerformanceMetricsDispatchCategory);
        total += item.count;
      }
    }

    Assert.ok(duration > 0, "Duration should be positive");
    Assert.ok(total > 0, "Should get a positive count");
    Assert.ok(parent_process_event, "parent process sent back some events");
  });

  SpecialPowers.clearUserPref('dom.performance.enable_scheduler_timing');
});
