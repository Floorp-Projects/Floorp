/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test migrated from
// devtools/client/aboutdebugging/test/browser_service_workers_not_compatible.js

const TEST_DATA = [
  {
    serviceWorkersEnabled: true,
    privateBrowsingEnabled: false,
    expectedMessage: false,
  },
  {
    serviceWorkersEnabled: false,
    privateBrowsingEnabled: false,
    expectedMessage: true,
  },
  {
    serviceWorkersEnabled: true,
    privateBrowsingEnabled: true,
    expectedMessage: true,
  },
  {
    serviceWorkersEnabled: false,
    privateBrowsingEnabled: true,
    expectedMessage: true,
  },
];

/**
 * Check that the warning message for service workers is displayed if permanent private
 * browsing is enabled or/and if service workers are disabled.
 */
add_task(async function testLocalRuntime() {
  for (const testData of TEST_DATA) {
    const {
      serviceWorkersEnabled,
      privateBrowsingEnabled,
      expectedMessage,
    } = testData;

    info(
      `Test warning message on this-firefox ` +
        `with serviceWorkersEnabled: ${serviceWorkersEnabled} ` +
        `and with privateBrowsingEnabled: ${privateBrowsingEnabled}`
    );

    await pushPref("dom.serviceWorkers.enabled", serviceWorkersEnabled);
    await pushPref("browser.privatebrowsing.autostart", privateBrowsingEnabled);

    const { document, tab, window } = await openAboutDebugging({
      // Even though this is a service worker test, we are not adding/removing
      // workers here. Since the test is really fast it can create intermittent
      // failures due to pending requests to update the worker list
      // We are updating the worker list whenever the list of processes changes
      // and this can happen very frequently, and it's hard to control from
      // DevTools.
      // Set enableWorkerUpdates to false to avoid intermittent failures.
      enableWorkerUpdates: false,
    });
    await selectThisFirefoxPage(document, window.AboutDebugging.store);
    assertWarningMessage(document, expectedMessage);
    await removeTab(tab);
  }
});

add_task(async function testRemoteRuntime() {
  const {
    remoteClientManager,
  } = require("devtools/client/shared/remote-debugging/remote-client-manager");

  // enable USB devices mocks
  const mocks = new Mocks();
  const client = mocks.createUSBRuntime("1337id", {
    deviceName: "Fancy Phone",
    name: "Lorem ipsum",
  });

  for (const testData of TEST_DATA) {
    const {
      serviceWorkersEnabled,
      privateBrowsingEnabled,
      expectedMessage,
    } = testData;

    info(
      `Test warning message on mocked USB runtime ` +
        `with serviceWorkersEnabled: ${serviceWorkersEnabled} ` +
        `and with privateBrowsingEnabled: ${privateBrowsingEnabled}`
    );

    client.setPreference("dom.serviceWorkers.enabled", serviceWorkersEnabled);
    client.setPreference(
      "browser.privatebrowsing.autostart",
      privateBrowsingEnabled
    );

    const { document, tab, window } = await openAboutDebugging({
      enableWorkerUpdates: false,
    });
    await selectThisFirefoxPage(document, window.AboutDebugging.store);

    info("Checking a USB runtime");
    mocks.emitUSBUpdate();
    await connectToRuntime("Fancy Phone", document);
    await selectRuntime("Fancy Phone", "Lorem ipsum", document);

    assertWarningMessage(document, expectedMessage);

    // We remove all clients in order to be able to simply connect to the runtime at
    // every iteration of the loop without checking of the runtime is already connected.
    info("Remove all remote clients");
    await remoteClientManager.removeAllClients();

    await removeTab(tab);
  }
});

function assertWarningMessage(doc, expectedMessage) {
  const hasMessage = !!doc.querySelector(".qa-service-workers-warning");
  ok(
    hasMessage === expectedMessage,
    expectedMessage
      ? "Warning message is displayed"
      : "Warning message is not displayed"
  );
}
