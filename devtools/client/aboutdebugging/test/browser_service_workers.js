/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Service workers can't be loaded from chrome://,
// but http:// is ok with dom.serviceWorkers.testing.enabled turned on.
const SERVICE_WORKER = URL_ROOT + "service-workers/empty-sw.js";
const TAB_URL = URL_ROOT + "service-workers/empty-sw.html";

add_task(async function() {
  await enableServiceWorkerDebugging();

  const { tab, document } = await openAboutDebugging("workers");

  const swTab = await addTab(TAB_URL);

  const serviceWorkersElement = getServiceWorkerList(document);

  await waitUntil(() => {
    // Check that the service worker appears in the UI
    let names = [...document.querySelectorAll("#service-workers .target-name")];
    names = names.map(element => element.textContent);
    return names.includes(SERVICE_WORKER);
  });
  info("The service worker url appears in the list");

  try {
    await unregisterServiceWorker(swTab, serviceWorkersElement);
    ok(true, "Service worker registration unregistered");
  } catch (e) {
    ok(false, "SW not unregistered; " + e);
  }

  // Check that the service worker disappeared from the UI
  let names = [...document.querySelectorAll("#service-workers .target-name")];
  names = names.map(element => element.textContent);
  ok(!names.includes(SERVICE_WORKER),
    "The service worker url is no longer in the list: " + names);

  await removeTab(swTab);
  await closeAboutDebugging(tab);
});
