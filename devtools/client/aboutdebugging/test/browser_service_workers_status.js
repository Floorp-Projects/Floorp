/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Service workers can't be loaded from chrome://,
// but http:// is ok with dom.serviceWorkers.testing.enabled turned on.
const SERVICE_WORKER = URL_ROOT + "service-workers/delay-sw.js";
const TAB_URL = URL_ROOT + "service-workers/delay-sw.html";
const SW_TIMEOUT = 2000;

requestLongerTimeout(2);

add_task(function* () {
  yield SpecialPowers.pushPrefEnv({
    "set": [
      // Accept workers from mochitest's http.
      ["dom.serviceWorkers.testing.enabled", true],
      ["dom.serviceWorkers.idle_timeout", SW_TIMEOUT],
      ["dom.serviceWorkers.idle_extended_timeout", SW_TIMEOUT],
      ["dom.ipc.processCount", 1],
    ]
  });

  let { tab, document } = yield openAboutDebugging("workers");

  // Listen for mutations in the service-workers list.
  let serviceWorkersElement = getServiceWorkerList(document);
  let onMutation = waitForMutation(serviceWorkersElement, { childList: true });

  let swTab = yield addTab(TAB_URL);

  info("Make the test page notify us when the service worker sends a message.");

  // Wait for the service-workers list to update.
  yield onMutation;

  // Check that the service worker appears in the UI
  let names = [...document.querySelectorAll("#service-workers .target-name")];
  let name = names.filter(element => element.textContent === SERVICE_WORKER)[0];
  ok(name, "Found the service worker in the list");

  let targetElement = name.parentNode.parentNode;
  let status = targetElement.querySelector(".target-status");
  is(status.textContent, "Registering", "Service worker is currently registering");

  yield waitForMutation(serviceWorkersElement, { childList: true, subtree: true });
  is(status.textContent, "Running", "Service worker is currently running");

  yield waitForMutation(serviceWorkersElement, { attributes: true, subtree: true });
  is(status.textContent, "Stopped", "Service worker is currently stopped");

  try {
    yield unregisterServiceWorker(swTab, serviceWorkersElement);
    ok(true, "Service worker unregistered");
  } catch (e) {
    ok(false, "Service worker not unregistered; " + e);
  }

  // Check that the service worker disappeared from the UI
  names = [...document.querySelectorAll("#service-workers .target-name")];
  names = names.map(element => element.textContent);
  ok(!names.includes(SERVICE_WORKER),
    "The service worker url is no longer in the list: " + names);

  yield removeTab(swTab);
  yield closeAboutDebugging(tab);
});
