"use strict";

const { interfaces: Ci } = Components;

const BASE_URI = "http://mochi.test:8888/browser/dom/workers/test/serviceworkers/";
const PAGE_URI = BASE_URI + "empty.html";
const SCOPE = PAGE_URI + "?storage_permission";
const SW_SCRIPT = BASE_URI + "empty.js";


add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({"set": [
    ["dom.serviceWorkers.enabled", true],
    ["dom.serviceWorkers.testing.enabled", true],
  ]});

  let tab = BrowserTestUtils.addTab(gBrowser, PAGE_URI);
  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  await ContentTask.spawn(browser, { script: SW_SCRIPT, scope: SCOPE },
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

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_allow_permission() {
  Services.perms.add(Services.io.newURI(PAGE_URI), "cookie",
                     Ci.nsICookiePermission.ACCESS_ALLOW);

  let tab = BrowserTestUtils.addTab(gBrowser, SCOPE);
  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  let controller = await ContentTask.spawn(browser, null, async function() {
    return content.navigator.serviceWorker.controller;
  });

  ok(!!controller, "page should be controlled with storage allowed");

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_deny_permission() {
  Services.perms.add(Services.io.newURI(PAGE_URI), "cookie",
                     Ci.nsICookiePermission.ACCESS_DENY);

  let tab = BrowserTestUtils.addTab(gBrowser, SCOPE);
  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  let controller = await ContentTask.spawn(browser, null, async function() {
    return content.navigator.serviceWorker.controller;
  });

  is(controller, null, "page should be not controlled with storage denied");

  await BrowserTestUtils.removeTab(tab);
  Services.perms.remove(Services.io.newURI(PAGE_URI), "cookie");
});

add_task(async function test_session_permission() {
  Services.perms.add(Services.io.newURI(PAGE_URI), "cookie",
                     Ci.nsICookiePermission.ACCESS_SESSION);

  let tab = BrowserTestUtils.addTab(gBrowser, SCOPE);
  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  let controller = await ContentTask.spawn(browser, null, async function() {
    return content.navigator.serviceWorker.controller;
  });

  is(controller, null, "page should be not controlled with session storage");

  await BrowserTestUtils.removeTab(tab);
  Services.perms.remove(Services.io.newURI(PAGE_URI), "cookie");
});

add_task(async function cleanup() {
  Services.perms.remove(Services.io.newURI(PAGE_URI), "cookie");

  let tab = BrowserTestUtils.addTab(gBrowser, PAGE_URI);
  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  await ContentTask.spawn(browser, SCOPE, async function(uri) {
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

  await BrowserTestUtils.removeTab(tab);
});
