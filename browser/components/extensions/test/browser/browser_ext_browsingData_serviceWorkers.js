/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.serviceWorkers.exemptFromPerDomainMax", true],
          ["dom.serviceWorkers.enabled", true],
          ["dom.serviceWorkers.testing.enabled", true]],
  });
});

add_task(async function testServiceWorkers() {
  function background() {
    const PAGE = "/browser/browser/components/extensions/test/browser/file_serviceWorker.html";

    browser.runtime.onMessage.addListener(msg => {
      browser.test.sendMessage("serviceWorkerRegistered");
    });

    browser.test.onMessage.addListener(async (msg) => {
      await browser.browsingData.remove({}, {serviceWorkers: true});
      browser.test.sendMessage("serviceWorkersRemoved");
    });

    // Create two serviceWorkers.
    browser.tabs.create({url: `http://mochi.test:8888${PAGE}`});
    browser.tabs.create({url: `http://example.com${PAGE}`});
  }

  function contentScript() {
    window.addEventListener("message", msg => { // eslint-disable-line mozilla/balanced-listeners
      if (msg.data == "serviceWorkerRegistered") {
        browser.runtime.sendMessage("serviceWorkerRegistered");
      }
    }, true);
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["browsingData", "tabs"],
      "content_scripts": [{
        "matches": [
          "http://mochi.test/*/file_serviceWorker.html",
          "http://example.com/*/file_serviceWorker.html",
        ],
        "js": ["script.js"],
        "run_at": "document_start",
      }],
    },
    files: {
      "script.js": contentScript,
    },
  });

  let serviceWorkerManager = SpecialPowers.Cc["@mozilla.org/serviceworkers/manager;1"]
    .getService(SpecialPowers.Ci.nsIServiceWorkerManager);

  let win = await BrowserTestUtils.openNewBrowserWindow();
  await focusWindow(win);

  await extension.startup();
  await extension.awaitMessage("serviceWorkerRegistered");
  await extension.awaitMessage("serviceWorkerRegistered");

  let serviceWorkers = [];
  // Even though we await the registrations by waiting for the messages,
  // sometimes the serviceWorkers are still not registered at this point.
  while (serviceWorkers.length < 2) {
    serviceWorkers = serviceWorkerManager.getAllRegistrations();
    await new Promise(resolve => setTimeout(resolve, 1));
  }
  is(serviceWorkers.length, 2, "ServiceWorkers have been registered.");

  extension.sendMessage();

  await extension.awaitMessage("serviceWorkersRemoved");

  // The serviceWorkers and not necessarily removed immediately.
  while (serviceWorkers.length > 0) {
    serviceWorkers = serviceWorkerManager.getAllRegistrations();
    await new Promise(resolve => setTimeout(resolve, 1));
  }
  is(serviceWorkers.length, 0, "ServiceWorkers have been removed.");

  await extension.unload();
  await BrowserTestUtils.closeWindow(win);
});
