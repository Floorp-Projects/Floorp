/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Service worker debugging is unavailable when multi-e10s is enabled.
// Check that the appropriate warning panel is displayed when there are more than 1
// content process available.

const SERVICE_WORKER = URL_ROOT + "service-workers/empty-sw.js";
const TAB_URL = URL_ROOT + "service-workers/empty-sw.html";

add_task(function* () {
  yield enableServiceWorkerDebugging();
  info("Force two content processes");
  yield pushPref("dom.ipc.processCount", 2);

  let { tab, document } = yield openAboutDebugging("workers");

  let warningSection = document.querySelector(".service-worker-multi-process");
  let img = warningSection.querySelector(".warning");
  ok(img, "warning message is rendered");

  let swTab = yield addTab(TAB_URL, { background: true });
  let serviceWorkersElement = getServiceWorkerList(document);

  yield waitForMutation(serviceWorkersElement, { childList: true });

  info("Check that service worker buttons are disabled.");
  // Check that the service worker appears in the UI
  let serviceWorkerContainer = getServiceWorkerContainer(SERVICE_WORKER, document);
  let debugButton = serviceWorkerContainer.querySelector(".debug-button");
  ok(debugButton.disabled, "Start/Debug button is disabled");

  info("Update the preference to 1");
  let onWarningCleared = waitUntil(() => {
    return document.querySelector(".service-worker-multi-process");
  });
  yield pushPref("dom.ipc.processCount", 1);
  yield onWarningCleared;
  ok(!debugButton.disabled, "Debug button is enabled.");

  info("Update the preference back to 2");
  let onWarningRestored = waitUntil(() => {
    return document.querySelector(".service-worker-multi-process");
  });
  yield pushPref("dom.ipc.processCount", 2);
  yield onWarningRestored;
  ok(debugButton.disabled, "Debug button is disabled again.");

  info("Unregister service worker");
  try {
    yield unregisterServiceWorker(swTab, serviceWorkersElement);
    ok(true, "Service worker registration unregistered");
  } catch (e) {
    ok(false, "SW not unregistered; " + e);
  }

  yield removeTab(swTab);
  yield closeAboutDebugging(tab);
});
