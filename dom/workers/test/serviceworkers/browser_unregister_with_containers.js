"use strict";

const { interfaces: Ci } = Components;

const BASE_URI = "http://mochi.test:8888/browser/dom/workers/test/serviceworkers/";
const PAGE_URI = BASE_URI + "empty.html";
const SCOPE = PAGE_URI + "?unregister_with_containers";
const SW_SCRIPT = BASE_URI + "empty.js";

function doRegister(browser) {
  return ContentTask.spawn(browser, { script: SW_SCRIPT, scope: SCOPE },
    async function(opts) {
      let reg = await content.navigator.serviceWorker.register(opts.script,
                                                               { scope: opts.scope });
      let worker = reg.installing || reg.waiting || reg.active;
      await new Promise(resolve => {
        if (worker.state === "activated") {
          resolve();
          return;
        }
        worker.addEventListener("statechange", function onStateChange() {
          if (worker.state === "activated") {
            worker.removeEventListener("statechange", onStateChange);
            resolve();
          }
        });
      });
    }
  );
}

function doUnregister(browser) {
  return ContentTask.spawn(browser, SCOPE, async function(uri) {
    let reg = await content.navigator.serviceWorker.getRegistration(uri);
    let worker = reg.active;
    await reg.unregister();
    await new Promise(resolve => {
      if (worker.state === "redundant") {
        resolve();
        return;
      }
      worker.addEventListener("statechange", function onStateChange() {
        if (worker.state === "redundant") {
          worker.removeEventListener("statechange", onStateChange);
          resolve();
        }
      });
    });
  });
}

function isControlled(browser) {
  return ContentTask.spawn(browser, null, function() {
    return !!content.navigator.serviceWorker.controller;
  });
}

async function checkControlled(browser) {
  let controlled = await isControlled(browser);
  ok(controlled, "window should be controlled");
}

async function checkUncontrolled(browser) {
  let controlled = await isControlled(browser);
  ok(!controlled, "window should not be controlled");
}

add_task(async function test() {
  await SpecialPowers.pushPrefEnv({"set": [
    // Avoid service worker propagation races by disabling multi-e10s for now.
    // This can be removed after the e10s refactor is complete.
    ["dom.ipc.processCount", 1],
    ["dom.serviceWorkers.enabled", true],
    ["dom.serviceWorkers.testing.enabled", true],
  ]});

  // Setup service workers in two different contexts with the same scope.
  let containerTab1 = BrowserTestUtils.addTab(gBrowser, PAGE_URI, { userContextId: 1 });
  let containerBrowser1 = gBrowser.getBrowserForTab(containerTab1);
  await BrowserTestUtils.browserLoaded(containerBrowser1);

  let containerTab2 = BrowserTestUtils.addTab(gBrowser, PAGE_URI, { userContextId: 2 });
  let containerBrowser2 = gBrowser.getBrowserForTab(containerTab2);
  await BrowserTestUtils.browserLoaded(containerBrowser2);

  await doRegister(containerBrowser1);
  await doRegister(containerBrowser2);

  await checkUncontrolled(containerBrowser1);
  await checkUncontrolled(containerBrowser2);

  // Close the tabs we used to register the service workers.  These are not
  // controlled.
  await BrowserTestUtils.removeTab(containerTab1);
  await BrowserTestUtils.removeTab(containerTab2);

  // Open a controlled tab in each container.
  containerTab1 = BrowserTestUtils.addTab(gBrowser, SCOPE, { userContextId: 1 });
  containerBrowser1 = gBrowser.getBrowserForTab(containerTab1);
  await BrowserTestUtils.browserLoaded(containerBrowser1);

  containerTab2 = BrowserTestUtils.addTab(gBrowser, SCOPE, { userContextId: 2 });
  containerBrowser2 = gBrowser.getBrowserForTab(containerTab2);
  await BrowserTestUtils.browserLoaded(containerBrowser2);

  await checkControlled(containerBrowser1);
  await checkControlled(containerBrowser2);

  // Remove the first container's controlled tab
  await BrowserTestUtils.removeTab(containerTab1);

  // Create a new uncontrolled tab for the first container and use it to
  // unregister the service worker.
  containerTab1 = BrowserTestUtils.addTab(gBrowser, PAGE_URI, { userContextId: 1 });
  containerBrowser1 = gBrowser.getBrowserForTab(containerTab1);
  await BrowserTestUtils.browserLoaded(containerBrowser1);
  await doUnregister(containerBrowser1);

  await checkUncontrolled(containerBrowser1);
  await checkControlled(containerBrowser2);

  // Remove the second container's controlled tab
  await BrowserTestUtils.removeTab(containerTab2);

  // Create a new uncontrolled tab for the second container and use it to
  // unregister the service worker.
  containerTab2 = BrowserTestUtils.addTab(gBrowser, PAGE_URI, { userContextId: 2 });
  containerBrowser2 = gBrowser.getBrowserForTab(containerTab2);
  await BrowserTestUtils.browserLoaded(containerBrowser2);
  await doUnregister(containerBrowser2);

  await checkUncontrolled(containerBrowser1);
  await checkUncontrolled(containerBrowser2);

  // Close the two tabs we used to unregister the service worker.
  await BrowserTestUtils.removeTab(containerTab1);
  await BrowserTestUtils.removeTab(containerTab2);
});
