/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-serviceworker.js */
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "helper-serviceworker.js",
  this
);
/* import-globals-from helper-collapsibilities.js */
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "helper-collapsibilities.js",
  this
);

const SW_TAB_URL = URL_ROOT + "resources/service-workers/push-sw.html";
const SW_URL = URL_ROOT + "resources/service-workers/push-sw.js";

// This is a migration from:
// https://searchfox.org/mozilla-central/source/devtools/client/aboutdebugging/test/browser_service_workers.js

/**
 * Test that service workers appear and dissapear from the runtime page when they
 * are registered / unregistered.
 */
add_task(async function() {
  prepareCollapsibilitiesTest();
  await enableServiceWorkerDebugging();
  const { document, tab, window } = await openAboutDebugging({
    enableWorkerUpdates: true,
  });
  const store = window.AboutDebugging.store;

  await selectThisFirefoxPage(document, store);

  // check that SW list is empty
  info("Check that the SW pane is empty");
  let swPane = getDebugTargetPane("Service Workers", document);
  ok(!swPane.querySelector(".qa-debug-target-item"), "SW list is empty");

  // open a tab and register service worker
  info("Register a service worker");
  const swTab = await addTab(SW_TAB_URL);

  // check that service worker is rendered
  info("Wait until the service worker appears and is running");
  await waitForServiceWorkerRunning(SW_URL, document);

  swPane = getDebugTargetPane("Service Workers", document);
  ok(
    swPane.querySelectorAll(".qa-debug-target-item").length === 1,
    "Service worker list has one element"
  );
  ok(
    swPane.querySelector(".qa-debug-target-item").textContent.includes(SW_URL),
    "Service worker list is the one we registered"
  );

  // unregister the service worker
  info("Unregister service worker");
  await unregisterServiceWorker(swTab);
  // check that service worker is not rendered anymore
  info("Wait for service worker to disappear");
  await waitUntil(() => {
    swPane = getDebugTargetPane("Service Workers", document);
    return swPane.querySelectorAll(".qa-debug-target-item").length === 0;
  });

  info("Remove tabs");
  await removeTab(swTab);
  await removeTab(tab);
});
