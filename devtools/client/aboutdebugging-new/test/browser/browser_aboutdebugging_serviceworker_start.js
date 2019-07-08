/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-serviceworker.js */
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "helper-serviceworker.js",
  this
);

const SW_TAB_URL = URL_ROOT + "resources/service-workers/empty-sw.html";
const SW_URL = URL_ROOT + "resources/service-workers/empty-sw.js";

// This is a migration from:
// https://searchfox.org/mozilla-central/source/devtools/client/aboutdebugging/test/browser_service_workers_start.js

/**
 * Test that service workers can be started using about:debugging.
 */
add_task(async function() {
  await enableServiceWorkerDebugging();

  // Setting a low idle_timeout and idle_extended_timeout will allow the service worker
  // to reach the STOPPED state quickly, which will allow us to test the start button.
  // The default value is 30000 milliseconds.
  info("Set a low service worker idle timeout");
  await pushPref("dom.serviceWorkers.idle_timeout", 1000);
  await pushPref("dom.serviceWorkers.idle_extended_timeout", 1000);

  const { document, tab, window } = await openAboutDebugging({
    enableWorkerUpdates: true,
  });
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  // Open a tab that registers a basic service worker.
  const swTab = await addTab(SW_TAB_URL);

  // Wait for the registration to make sure service worker has been started, and that we
  // are not just reading STOPPED as the initial state.
  await waitForRegistration(swTab);

  info("Wait until the service worker stops");
  const targetElement = await waitForServiceWorkerStopped(SW_URL, document);

  // Retrieve the Start button for the worker.
  const startButton = targetElement.querySelector(".qa-start-button");
  ok(startButton, "Found its start button");

  info(
    "Click on the start button and wait for the service worker to be running"
  );
  const onServiceWorkerRunning = waitForServiceWorkerRunning(SW_URL, document);
  startButton.click();
  const updatedTarget = await onServiceWorkerRunning;

  // Check that the buttons are displayed as expected.
  const hasInspectButton = updatedTarget.querySelector(
    ".qa-debug-target-inspect-button"
  );
  const hasStartButton = updatedTarget.querySelector(".qa-start-button");
  ok(hasInspectButton, "Service worker has an inspect button");
  ok(!hasStartButton, "Service worker does not have a start button");

  info("Unregister service worker");
  await unregisterServiceWorker(swTab);

  info("Wait until the service worker disappears from about:debugging");
  await waitUntil(() => !findDebugTargetByText(SW_URL, document));

  info("Remove tabs");
  await removeTab(swTab);
  await removeTab(tab);
});
