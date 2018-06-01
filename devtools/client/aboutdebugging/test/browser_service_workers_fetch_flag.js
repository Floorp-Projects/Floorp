/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Service workers can't be loaded from chrome://,
// but http:// is ok with dom.serviceWorkers.testing.enabled turned on.
const EMPTY_SW_TAB_URL = URL_ROOT + "service-workers/empty-sw.html";
const FETCH_SW_TAB_URL = URL_ROOT + "service-workers/fetch-sw.html";

async function testBody(url, expecting) {
  await enableServiceWorkerDebugging();
  const { tab, document } = await openAboutDebugging("workers");

  const swTab = await addTab(url);

  const serviceWorkersElement = getServiceWorkerList(document);

  info("Wait for fetch flag.");
  await waitUntil(() => {
    let fetchFlags =
      [...document.querySelectorAll("#service-workers .service-worker-fetch-flag")];
    fetchFlags = fetchFlags.map(element => element.textContent);
    return fetchFlags.includes(expecting);
  }, 100);

  info("Found correct fetch flag.");

  try {
    await unregisterServiceWorker(swTab, serviceWorkersElement);
    ok(true, "Service worker registration unregistered");
  } catch (e) {
    ok(false, "SW not unregistered; " + e);
  }

  // Check that the service worker disappeared from the UI
  let names = [...document.querySelectorAll("#service-workers .target-name")];
  names = names.map(element => element.textContent);
  ok(names.length == 0, "All service workers were removed from the list.");

  await removeTab(swTab);
  await closeAboutDebugging(tab);
}

add_task(async function() {
  await testBody(FETCH_SW_TAB_URL, "Listening for fetch events.");
  await testBody(EMPTY_SW_TAB_URL, "Not listening for fetch events.");
});
