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
  yield enableServiceWorkerDebugging();
  yield pushPref("dom.serviceWorkers.idle_timeout", SW_TIMEOUT);
  yield pushPref("dom.serviceWorkers.idle_extended_timeout", SW_TIMEOUT);

  let { tab, document } = yield openAboutDebugging("workers");

  // Listen for mutations in the service-workers list.
  let serviceWorkersElement = getServiceWorkerList(document);

  let swTab = yield addTab(TAB_URL, { background: true });

  info("Wait until the service worker appears in about:debugging");
  let container = yield waitUntilServiceWorkerContainer(SERVICE_WORKER, document);

  // We should ideally check that the service worker registration goes through the
  // "registering" and "running" steps, but it is difficult to workaround race conditions
  // for a test running on a wide variety of platforms. Due to intermittent failures, we
  // simply check that the registration transitions to "stopped".
  let status = container.querySelector(".target-status");
  yield waitUntil(() => status.textContent == "Stopped", 100);
  is(status.textContent, "Stopped", "Service worker is currently stopped");

  try {
    yield unregisterServiceWorker(swTab, serviceWorkersElement);
    ok(true, "Service worker unregistered");
  } catch (e) {
    ok(false, "Service worker not unregistered; " + e);
  }

  // Check that the service worker disappeared from the UI
  let names = [...document.querySelectorAll("#service-workers .target-name")];
  names = names.map(element => element.textContent);
  ok(!names.includes(SERVICE_WORKER),
    "The service worker url is no longer in the list: " + names);

  yield removeTab(swTab);
  yield closeAboutDebugging(tab);
});
