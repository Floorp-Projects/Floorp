/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-serviceworker.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-serviceworker.js", this);

const SW_TAB_URL = URL_ROOT + "resources/service-workers/empty-sw.html";
const SW_URL = URL_ROOT + "resources/service-workers/empty-sw.js";

// This is a migration from:
// https://searchfox.org/mozilla-central/source/devtools/client/aboutdebugging/test/browser_service_workers_unregister.js

/**
 * Test that service workers can be started using about:debugging.
 */
add_task(async function() {
  await enableServiceWorkerDebugging();

  const { document, tab, window } =
    await openAboutDebugging({ enableWorkerUpdates: true });
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  // Open a tab that registers a basic service worker.
  const swTab = await addTab(SW_TAB_URL);

  info("Wait until the service worker appears and is running");
  const targetElement = await waitForServiceWorkerRunning(SW_URL, document);

  // Retrieve the Start button for the worker.
  const unregisterButton = targetElement.querySelector(".qa-unregister-button");
  ok(unregisterButton, "Found its unregister button");

  info("Click on the unregister button and wait for the service worker to disappear");
  unregisterButton.click();
  await waitUntil(() => !findDebugTargetByText(SW_URL, document));

  const hasServiceWorkerTarget = !!findDebugTargetByText(SW_URL, document);
  ok(!hasServiceWorkerTarget, "Service worker was successfully unregistered");

  info("Remove tabs");
  await removeTab(swTab);
  await removeTab(tab);
});
