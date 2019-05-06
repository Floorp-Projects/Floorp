/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test will be idle for a long period to give a chance to the service worker to
// timeout.
requestLongerTimeout(3);

/* import-globals-from helper-serviceworker.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-serviceworker.js", this);

const SW_TAB_URL = URL_ROOT + "resources/service-workers/empty-sw.html";
const SW_URL = URL_ROOT + "resources/service-workers/empty-sw.js";
const SW_TIMEOUT = 4000;

// This is a migration from:
// https://searchfox.org/mozilla-central/source/devtools/client/aboutdebugging/test/browser_service_workers_timeout.js

/**
 * Test that service workers will _not_ timeout and be stopped when a toolbox is attached
 * to them. Feature implemented in Bug 1228382.
 */
add_task(async function() {
  await enableServiceWorkerDebugging();

  // Setting a low idle_timeout and idle_extended_timeout will allow the service worker
  // to reach the STOPPED state quickly, which will allow us to test the start button.
  // The default value is 30000 milliseconds.
  info("Set a low service worker idle timeout");
  await pushPref("dom.serviceWorkers.idle_timeout", SW_TIMEOUT);
  await pushPref("dom.serviceWorkers.idle_extended_timeout", SW_TIMEOUT);

  const { document, tab, window } =
    await openAboutDebugging({ enableWorkerUpdates: true });
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  // Open a tab that registers a basic service worker.
  const swTab = await addTab(SW_TAB_URL);

  // Wait for the registration to make sure service worker has been started, and that we
  // are not just reading STOPPED as the initial state.
  await waitForRegistration(swTab);

  info("Wait until the service worker stops");
  await waitForServiceWorkerStopped(SW_URL, document);

  info("Click on the start button and wait for the service worker to be running");
  const onServiceWorkerRunning = waitForServiceWorkerRunning(SW_URL, document);
  const startButton = getStartButton(SW_URL, document);
  startButton.click();
  await onServiceWorkerRunning;

  const inspectButton = getInspectButton(SW_URL, document);
  ok(!!inspectButton, "Service worker target has an inspect button");

  info("Click on inspect and wait for the toolbox to open");
  const onToolboxReady = gDevTools.once("toolbox-ready");
  inspectButton.click();
  await onToolboxReady;

  // Wait for more 5 times the service worker timeout to check that the toolbox prevents
  // the worker from being destroyed.
  await wait(SW_TIMEOUT * 5);

  // Check that the service worker is still running, even after waiting 5 times the
  // service worker timeout.
  const hasInspectButton = !!getInspectButton(SW_URL, document);
  ok(hasInspectButton, "Service worker target still has an inspect button");

  info("Destroy the toolbox");
  const devtoolsTab = gBrowser.selectedTab;
  await closeAboutDevtoolsToolbox(document, devtoolsTab, window);

  // After stopping the toolbox, the service worker instance should be released and the
  // service worker registration should be displayed as stopped again.
  info("Wait until the service worker stops after closing the toolbox");
  await waitForServiceWorkerStopped(SW_URL, document);

  info("Unregister service worker");
  await unregisterServiceWorker(swTab);

  info("Wait until the service worker disappears from about:debugging");
  await waitUntil(() => !findDebugTargetByText(SW_URL, document));

  info("Remove tabs");
  await removeTab(swTab);
  await removeTab(tab);
});

function getStartButton(workerText, doc) {
  const target = findDebugTargetByText(workerText, doc);
  return target ? target.querySelector(".qa-start-button") : null;
}

function getInspectButton(workerText, doc) {
  const target = findDebugTargetByText(workerText, doc);
  return target ? target.querySelector(".qa-debug-target-inspect-button") : null;
}
