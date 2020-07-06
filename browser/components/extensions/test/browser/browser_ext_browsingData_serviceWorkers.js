/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.serviceWorkers.exemptFromPerDomainMax", true],
      ["dom.serviceWorkers.enabled", true],
      ["dom.serviceWorkers.testing.enabled", true],
    ],
  });
});

add_task(async function testServiceWorkers() {
  function background() {
    const PAGE =
      "/browser/browser/components/extensions/test/browser/file_serviceWorker.html";

    browser.runtime.onMessage.addListener(msg => {
      browser.test.sendMessage("serviceWorkerRegistered");
    });

    browser.test.onMessage.addListener(async msg => {
      await browser.browsingData.remove(
        { hostnames: msg.hostnames },
        { serviceWorkers: true }
      );
      browser.test.sendMessage("serviceWorkersRemoved");
    });

    // Create two serviceWorkers.
    browser.tabs.create({ url: `http://mochi.test:8888${PAGE}` });
    browser.tabs.create({ url: `http://example.com${PAGE}` });
  }

  function contentScript() {
    // eslint-disable-next-line mozilla/balanced-listeners
    window.addEventListener(
      "message",
      msg => {
        if (msg.data == "serviceWorkerRegistered") {
          browser.runtime.sendMessage("serviceWorkerRegistered");
        }
      },
      true
    );
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["browsingData", "tabs"],
      content_scripts: [
        {
          matches: [
            "http://mochi.test/*/file_serviceWorker.html",
            "http://example.com/*/file_serviceWorker.html",
          ],
          js: ["script.js"],
          run_at: "document_start",
        },
      ],
    },
    files: {
      "script.js": contentScript,
    },
  });

  let serviceWorkerManager = SpecialPowers.Cc[
    "@mozilla.org/serviceworkers/manager;1"
  ].getService(SpecialPowers.Ci.nsIServiceWorkerManager);

  let win = await BrowserTestUtils.openNewBrowserWindow();
  await focusWindow(win);

  await extension.startup();
  await extension.awaitMessage("serviceWorkerRegistered");
  await extension.awaitMessage("serviceWorkerRegistered");

  // Even though we await the registrations by waiting for the messages,
  // sometimes the serviceWorkers are still not registered at this point.
  async function getRegistrations(count) {
    await BrowserTestUtils.waitForCondition(
      () => serviceWorkerManager.getAllRegistrations().length === count,
      `Wait for ${count} service workers to be registered`
    );
    return serviceWorkerManager.getAllRegistrations();
  }

  let serviceWorkers = await getRegistrations(2);
  is(serviceWorkers.length, 2, "ServiceWorkers have been registered.");

  extension.sendMessage({ hostnames: ["example.com"] });
  await extension.awaitMessage("serviceWorkersRemoved");

  serviceWorkers = await getRegistrations(1);
  is(
    serviceWorkers.length,
    1,
    "ServiceWorkers for example.com have been removed."
  );
  let host = serviceWorkers.queryElementAt(
    0,
    Ci.nsIServiceWorkerRegistrationInfo
  ).principal.URI.host;
  is(host, "mochi.test", "ServiceWorkers for example.com have been removed.");

  extension.sendMessage({});
  await extension.awaitMessage("serviceWorkersRemoved");

  serviceWorkers = await getRegistrations(0);
  is(serviceWorkers.length, 0, "All ServiceWorkers have been removed.");

  await extension.unload();
  await BrowserTestUtils.closeWindow(win);
});
