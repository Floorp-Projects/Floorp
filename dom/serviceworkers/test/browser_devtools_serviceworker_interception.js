"use strict";

const { classes: Cc, interfaces: Ci, results: Cr } = Components;

const BASE_URI =
  "http://mochi.test:8888/browser/dom/workers/test/serviceworkers/";
const emptyDoc = BASE_URI + "empty.html";
const fakeDoc = BASE_URI + "fake.html";
const helloDoc = BASE_URI + "hello.html";

const CROSS_URI = "http://example.com/browser/dom/workers/test/serviceworkers/";
const crossRedirect = CROSS_URI + "redirect";
const crossHelloDoc = CROSS_URI + "hello.html";

const sw = BASE_URI + "fetch.js";

// XXXtt: We should be able to move this check to chrome process after we move
// the interception logic to chrome process.
async function checkObserverInContent(aInput) {
  let interceptedChannel = null;

  // We always get two channels which receive the "http-on-stop-request"
  // notification if the service worker hijacks the request and respondWith an
  // another fetch. One is for the "outer" window request when the other one is
  // for the "inner" service worker request. Therefore, distinguish them by the
  // order.
  let waitForSecondOnStopRequest = aInput.intercepted;

  let promiseResolve;

  function observer(aSubject) {
    let channel = aSubject.QueryInterface(Ci.nsIChannel);
    // Since we cannot make sure that the network event triggered by the fetch()
    // in this testcase is the very next event processed by ObserverService, we
    // have to wait until we catch the one we want.
    if (!channel.URI.spec.includes(aInput.expectedURL)) {
      return;
    }

    if (waitForSecondOnStopRequest) {
      waitForSecondOnStopRequest = false;
      return;
    }

    // Wait for the service worker to intercept the request if it's expected to
    // be intercepted
    if (aInput.intercepted && interceptedChannel === null) {
      return;
    } else if (interceptedChannel) {
      ok(aInput.intercepted,
         "Service worker intercepted the channel as expected");
    } else {
      ok(!aInput.intercepted, "The channel doesn't be intercepted");
    }

    var tc = interceptedChannel
               ? interceptedChannel.QueryInterface(Ci.nsITimedChannel)
               : aSubject.QueryInterface(Ci.nsITimedChannel);

    // Check service worker related timings.
    var serviceWorkerTimings = [{start: tc.launchServiceWorkerStartTime,
                                 end:   tc.launchServiceWorkerEndTime},
                                {start: tc.dispatchFetchEventStartTime,
                                 end:   tc.dispatchFetchEventEndTime},
                                {start: tc.handleFetchEventStartTime,
                                 end:   tc.handleFetchEventEndTime}];
    if (aInput.swPresent) {
      serviceWorkerTimings.reduce((aPreviousTimings, aCurrentTimings) => {
        ok(aPreviousTimings.start !== 0, "Start time check.");
        ok(aPreviousTimings.start <= aCurrentTimings.start,
           "Start time order check.");
        ok(aPreviousTimings.end <= aCurrentTimings.end,
           "End time order check.");
        ok(aCurrentTimings.start <= aCurrentTimings.end,
           "Start time should be smaller than end time.");
        return aCurrentTimings;
      });
    } else {
      serviceWorkerTimings.forEach(aTimings => {
        is(aTimings.start, 0, "SW timings should be 0.");
        is(aTimings.end, 0, "SW timings should be 0.");
      });
    }

    // Check network related timings.
    var networkTimings = [tc.domainLookupStartTime,
                          tc.domainLookupEndTime,
                          tc.connectStartTime,
                          tc.connectEndTime,
                          tc.requestStartTime,
                          tc.responseStartTime,
                          tc.responseEndTime];
    if (aInput.fetch) {
      networkTimings.reduce((aPreviousTiming, aCurrentTiming) => {
        ok(aPreviousTiming <= aCurrentTiming, "Checking network timings");
        return aCurrentTiming;
      });
    } else {
      networkTimings.forEach(aTiming => is(aTiming, 0,
                                           "Network timings should be 0."));
    }

    interceptedChannel = null;
    Services.obs.removeObserver(observer, topic);
    promiseResolve();
  }

  function addInterceptedChannel(aSubject) {
    let channel = aSubject.QueryInterface(Ci.nsIChannel);
    if (!channel.URI.spec.includes(aInput.url)) {
      return;
    }

    // Hold the interceptedChannel until checking timing information.
    // Note: It's a interceptedChannel in the type of httpChannel
    interceptedChannel = channel;
    Services.obs.removeObserver(addInterceptedChannel, topic_SW);
  }

  const topic = "http-on-stop-request";
  const topic_SW = "service-worker-synthesized-response";

  Services.obs.addObserver(observer, topic);
  if (aInput.intercepted) {
    Services.obs.addObserver(addInterceptedChannel, topic_SW);
  }

  await new Promise(resolve => { promiseResolve = resolve; });
}

async function contentFetch(aURL) {
  if (aURL.includes("redirect")) {
    await content.window.fetch(aURL, { mode: "no-cors" });
    return;
  }
  await content.window.fetch(aURL);
}

async function registerSWAndWaitForActive(aServiceWorker) {
  let swr =
    await content.navigator.serviceWorker.register(aServiceWorker,
                                                   { scope:"empty.html" });
  await new Promise(resolve => {
    let worker = swr.installing || swr.waiting || swr.active;
    if (worker.state === 'activated') {
      return resolve();
    }

    worker.addEventListener('statechange', () => {
      if (worker.state === 'activated') {
        return resolve();
      }
    });
  });

  await swr.active.postMessage('claim');

  await new Promise(resolve => {
    if (content.navigator.serviceWorker.controller) {
      return resolve();
    }

    content.navigator.serviceWorker.addEventListener('controllerchange',
                                                     resolve, { once: true });
  });
}

async function unregisterSW() {
  let swr = await content.navigator.serviceWorker.getRegistration();
  swr.unregister();
}

add_task(async function test_serivce_worker_interception() {
  info("Setting the prefs to having e10s enabled");
  await SpecialPowers.pushPrefEnv({"set": [
    // Make sure observer and testing function run in the same process
    ["dom.ipc.processCount", 1],
    ["dom.serviceWorkers.enabled", true],
    ["dom.serviceWorkers.testing.enabled", true],
  ]});

  waitForExplicitFinish();

  info("Open the tab");
  let tab = BrowserTestUtils.addTab(gBrowser, emptyDoc);
  let tabBrowser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(tabBrowser);

  info("Open the tab for observing");
  let tab_observer = BrowserTestUtils.addTab(gBrowser, emptyDoc);
  let tabBrowser_observer = gBrowser.getBrowserForTab(tab_observer);
  await BrowserTestUtils.browserLoaded(tabBrowser_observer);

  let testcases = [
    {
      url: helloDoc,
      expectedURL: helloDoc,
      swPresent: false,
      intercepted: false,
      fetch: true
    },
    {
      url: fakeDoc,
      expectedURL: helloDoc,
      swPresent: true,
      intercepted: true,
      fetch: false // should use HTTP cache
    },
    { // Bypass http cache
      url: helloDoc + "?ForBypassingHttpCache=" + Date.now(),
      expectedURL: helloDoc,
      swPresent: true,
      intercepted: false,
      fetch: true
    },
    { // no-cors mode redirect to no-cors mode (trigger internal redirect)
      url: crossRedirect + "?url=" + crossHelloDoc + "&mode=no-cors",
      expectedURL: crossHelloDoc,
      swPresent: true,
      redirect: "hello.html",
      intercepted: true,
      fetch: true
    }
  ];

  info("Test 1: Verify simple fetch");
  let promise = ContentTask.spawn(tabBrowser_observer,
                                  testcases[0],
                                  checkObserverInContent);
  await ContentTask.spawn(tabBrowser, testcases[0].url, contentFetch);
  await promise;

  info("Register a service worker");
  await ContentTask.spawn(tabBrowser, sw, registerSWAndWaitForActive);

  info("Test 2: Verify simple hijack");
  promise = ContentTask.spawn(tabBrowser_observer,
                              testcases[1],
                              checkObserverInContent);
  await ContentTask.spawn(tabBrowser, testcases[1].url, contentFetch);
  await promise;

  info("Test 3: Verify fetch without using http cache");
  promise = ContentTask.spawn(tabBrowser_observer,
                              testcases[2],
                              checkObserverInContent);
  await ContentTask.spawn(tabBrowser, testcases[2].url, contentFetch);
  await promise;

  info("Test 4: make a internal redirect");
  promise = ContentTask.spawn(tabBrowser_observer,
                              testcases[3],
                              checkObserverInContent);
  await ContentTask.spawn(tabBrowser, testcases[3].url, contentFetch);
  await promise;

  info("Clean up");
  await ContentTask.spawn(tabBrowser, undefined, unregisterSW);

  gBrowser.removeTab(tab);
  gBrowser.removeTab(tab_observer);
});

