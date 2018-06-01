/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that clicking on the Start button next to a Service Worker works as
// intended in about:debugging.
// It should cause a worker to start running in a child process.

// Service workers can't be loaded from chrome://, but http:// is ok with
// dom.serviceWorkers.testing.enabled turned on.
const SERVICE_WORKER = URL_ROOT + "service-workers/empty-sw.js";
const TAB_URL = URL_ROOT + "service-workers/empty-sw.html";

const SW_TIMEOUT = 1000;

add_task(async function() {
  await enableServiceWorkerDebugging();
  await pushPref("dom.serviceWorkers.idle_timeout", SW_TIMEOUT);
  await pushPref("dom.serviceWorkers.idle_extended_timeout", SW_TIMEOUT);

  const { tab, document } = await openAboutDebugging("workers");

  // Listen for mutations in the service-workers list.
  const serviceWorkersElement = getServiceWorkerList(document);

  // Open a tab that registers an empty service worker.
  const swTab = await addTab(TAB_URL);

  // Wait for the service-workers list to update.
  info("Wait until the service worker appears in about:debugging");
  await waitUntilServiceWorkerContainer(SERVICE_WORKER, document);

  info("Ensure that the registration resolved before trying to interact with " +
    "the service worker.");
  await waitForServiceWorkerRegistered(swTab);
  ok(true, "Service worker registration resolved");

  await waitForServiceWorkerActivation(SERVICE_WORKER, document);

  // Retrieve the Target element corresponding to the service worker.
  const names = [...document.querySelectorAll("#service-workers .target-name")];
  const name = names.filter(element => element.textContent === SERVICE_WORKER)[0];
  ok(name, "Found the service worker in the list");
  const targetElement = name.parentNode.parentNode;

  // The service worker may already be killed with the low 1s timeout.
  // At this stage, either the SW is started and the Debug button is visible or was
  // already stopped and the start button is visible. Wait until the service worker is
  // stopped.
  info("Wait until the start button is visible");
  await waitUntilElement(".start-button", targetElement);

  // We should now have a Start button but no Debug button.
  const startBtn = targetElement.querySelector(".start-button");
  ok(startBtn, "Found its start button");
  ok(!targetElement.querySelector(".debug-button"), "No debug button");

  // Click on the Start button and wait for the service worker to be back.
  startBtn.click();

  info("Wait until the service worker starts and the debug button appears");
  await waitUntilElement(".debug-button", targetElement);
  info("Found the debug button");

  // Check that we have a Debug button but not a Start button again.
  ok(!targetElement.querySelector(".start-button"), "No start button");

  // Finally, unregister the service worker itself.
  try {
    await unregisterServiceWorker(swTab, serviceWorkersElement);
    ok(true, "Service worker registration unregistered");
  } catch (e) {
    ok(false, "SW not unregistered; " + e);
  }

  await removeTab(swTab);
  await closeAboutDebugging(tab);
});
