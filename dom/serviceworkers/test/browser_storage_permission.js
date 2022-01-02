"use strict";

const { PermissionTestUtils } = ChromeUtils.import(
  "resource://testing-common/PermissionTestUtils.jsm"
);

const BASE_URI = "http://mochi.test:8888/browser/dom/serviceworkers/test/";
const PAGE_URI = BASE_URI + "empty.html";
const SCOPE = PAGE_URI + "?storage_permission";
const SW_SCRIPT = BASE_URI + "empty.js";

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Until the e10s refactor is complete, use a single process to avoid
      // service worker propagation race.
      ["dom.ipc.processCount", 1],
      ["dom.serviceWorkers.enabled", true],
      ["dom.serviceWorkers.testing.enabled", true],
    ],
  });

  let tab = BrowserTestUtils.addTab(gBrowser, PAGE_URI);
  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  await SpecialPowers.spawn(
    browser,
    [{ script: SW_SCRIPT, scope: SCOPE }],
    async function(opts) {
      let reg = await content.navigator.serviceWorker.register(opts.script, {
        scope: opts.scope,
      });
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

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_allow_permission() {
  PermissionTestUtils.add(
    PAGE_URI,
    "cookie",
    Ci.nsICookiePermission.ACCESS_ALLOW
  );

  let tab = BrowserTestUtils.addTab(gBrowser, SCOPE);
  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  let controller = await SpecialPowers.spawn(browser, [], async function() {
    return content.navigator.serviceWorker.controller;
  });

  ok(!!controller, "page should be controlled with storage allowed");

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_deny_permission() {
  PermissionTestUtils.add(
    PAGE_URI,
    "cookie",
    Ci.nsICookiePermission.ACCESS_DENY
  );

  let tab = BrowserTestUtils.addTab(gBrowser, SCOPE);
  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  let controller = await SpecialPowers.spawn(browser, [], async function() {
    return content.navigator.serviceWorker.controller;
  });

  is(controller, null, "page should be not controlled with storage denied");

  BrowserTestUtils.removeTab(tab);
  PermissionTestUtils.remove(PAGE_URI, "cookie");
});

add_task(async function test_session_permission() {
  PermissionTestUtils.add(
    PAGE_URI,
    "cookie",
    Ci.nsICookiePermission.ACCESS_SESSION
  );

  let tab = BrowserTestUtils.addTab(gBrowser, SCOPE);
  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  let controller = await SpecialPowers.spawn(browser, [], async function() {
    return content.navigator.serviceWorker.controller;
  });

  is(controller, null, "page should be not controlled with session storage");

  BrowserTestUtils.removeTab(tab);
  PermissionTestUtils.remove(PAGE_URI, "cookie");
});

// Test to verify an about:blank iframe successfully inherits the
// parent's controller when storage is blocked between opening the
// parent page and creating the iframe.
add_task(async function test_block_storage_before_blank_iframe() {
  PermissionTestUtils.remove(PAGE_URI, "cookie");

  let tab = BrowserTestUtils.addTab(gBrowser, SCOPE);
  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  let controller = await SpecialPowers.spawn(browser, [], async function() {
    return content.navigator.serviceWorker.controller;
  });

  ok(!!controller, "page should be controlled with storage allowed");

  let controller2 = await SpecialPowers.spawn(browser, [], async function() {
    let f = content.document.createElement("iframe");
    content.document.body.appendChild(f);
    await new Promise(resolve => (f.onload = resolve));
    return !!f.contentWindow.navigator.serviceWorker.controller;
  });

  ok(!!controller2, "page should be controlled with storage allowed");

  await SpecialPowers.pushPrefEnv({
    set: [
      ["network.cookie.cookieBehavior", Ci.nsICookieService.BEHAVIOR_REJECT],
    ],
  });

  let controller3 = await SpecialPowers.spawn(browser, [], async function() {
    let f = content.document.createElement("iframe");
    content.document.body.appendChild(f);
    await new Promise(resolve => (f.onload = resolve));
    return !!f.contentWindow.navigator.serviceWorker.controller;
  });

  ok(!!controller3, "page should be controlled with storage allowed");

  await SpecialPowers.popPrefEnv();
  BrowserTestUtils.removeTab(tab);
});

// Test to verify a blob URL iframe successfully inherits the
// parent's controller when storage is blocked between opening the
// parent page and creating the iframe.
add_task(async function test_block_storage_before_blob_iframe() {
  PermissionTestUtils.remove(PAGE_URI, "cookie");

  let tab = BrowserTestUtils.addTab(gBrowser, SCOPE);
  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  let controller = await SpecialPowers.spawn(browser, [], async function() {
    return content.navigator.serviceWorker.controller;
  });

  ok(!!controller, "page should be controlled with storage allowed");

  let controller2 = await SpecialPowers.spawn(browser, [], async function() {
    let b = new content.Blob(["<!DOCTYPE html><html></html>"], {
      type: "text/html",
    });
    let f = content.document.createElement("iframe");
    // No need to call revokeObjectURL() since the window will be closed shortly.
    f.src = content.URL.createObjectURL(b);
    content.document.body.appendChild(f);
    await new Promise(resolve => (f.onload = resolve));
    return !!f.contentWindow.navigator.serviceWorker.controller;
  });

  ok(!!controller2, "page should be controlled with storage allowed");

  await SpecialPowers.pushPrefEnv({
    set: [
      ["network.cookie.cookieBehavior", Ci.nsICookieService.BEHAVIOR_REJECT],
    ],
  });

  let controller3 = await SpecialPowers.spawn(browser, [], async function() {
    let b = new content.Blob(["<!DOCTYPE html><html></html>"], {
      type: "text/html",
    });
    let f = content.document.createElement("iframe");
    // No need to call revokeObjectURL() since the window will be closed shortly.
    f.src = content.URL.createObjectURL(b);
    content.document.body.appendChild(f);
    await new Promise(resolve => (f.onload = resolve));
    return !!f.contentWindow.navigator.serviceWorker.controller;
  });

  ok(!!controller3, "page should be controlled with storage allowed");

  await SpecialPowers.popPrefEnv();
  BrowserTestUtils.removeTab(tab);
});

// Test to verify a blob worker script does not hit our service
// worker storage assertions when storage is blocked between opening
// the parent page and creating the worker.  Note, we cannot
// explicitly check if the worker is controlled since we don't expose
// WorkerNavigator.serviceWorkers.controller yet.
add_task(async function test_block_storage_before_blob_worker() {
  PermissionTestUtils.remove(PAGE_URI, "cookie");

  let tab = BrowserTestUtils.addTab(gBrowser, SCOPE);
  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  let controller = await SpecialPowers.spawn(browser, [], async function() {
    return content.navigator.serviceWorker.controller;
  });

  ok(!!controller, "page should be controlled with storage allowed");

  let scriptURL = await SpecialPowers.spawn(browser, [], async function() {
    let b = new content.Blob(
      ["self.postMessage(self.location.href);self.close()"],
      { type: "application/javascript" }
    );
    // No need to call revokeObjectURL() since the window will be closed shortly.
    let u = content.URL.createObjectURL(b);
    let w = new content.Worker(u);
    return await new Promise(resolve => {
      w.onmessage = e => resolve(e.data);
    });
  });

  ok(scriptURL.startsWith("blob:"), "blob URL worker should run");

  await SpecialPowers.pushPrefEnv({
    set: [
      ["network.cookie.cookieBehavior", Ci.nsICookieService.BEHAVIOR_REJECT],
    ],
  });

  let scriptURL2 = await SpecialPowers.spawn(browser, [], async function() {
    let b = new content.Blob(
      ["self.postMessage(self.location.href);self.close()"],
      { type: "application/javascript" }
    );
    // No need to call revokeObjectURL() since the window will be closed shortly.
    let u = content.URL.createObjectURL(b);
    let w = new content.Worker(u);
    return await new Promise(resolve => {
      w.onmessage = e => resolve(e.data);
    });
  });

  ok(scriptURL2.startsWith("blob:"), "blob URL worker should run");

  await SpecialPowers.popPrefEnv();
  BrowserTestUtils.removeTab(tab);
});

add_task(async function cleanup() {
  PermissionTestUtils.remove(PAGE_URI, "cookie");

  let tab = BrowserTestUtils.addTab(gBrowser, PAGE_URI);
  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  await SpecialPowers.spawn(browser, [SCOPE], async function(uri) {
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

  BrowserTestUtils.removeTab(tab);
});
