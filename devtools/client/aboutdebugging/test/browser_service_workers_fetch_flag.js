/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Service workers can't be loaded from chrome://,
// but http:// is ok with dom.serviceWorkers.testing.enabled turned on.
const EMPTY_SW_TAB_URL = URL_ROOT + "service-workers/empty-sw.html";
const FETCH_SW_TAB_URL = URL_ROOT + "service-workers/fetch-sw.html";

function* testBody(url, expecting) {
  yield enableServiceWorkerDebugging();
  let { tab, document } = yield openAboutDebugging("workers");

  let swTab = yield addTab(url);

  let serviceWorkersElement = getServiceWorkerList(document);
  yield waitForMutation(serviceWorkersElement, { childList: true });

  let fetchFlags =
    [...document.querySelectorAll("#service-workers .service-worker-fetch-flag")];
  fetchFlags = fetchFlags.map(element => element.textContent);
  ok(fetchFlags.includes(expecting), "Found correct fetch flag.");

  try {
    yield unregisterServiceWorker(swTab, serviceWorkersElement);
    ok(true, "Service worker registration unregistered");
  } catch (e) {
    ok(false, "SW not unregistered; " + e);
  }

  // Check that the service worker disappeared from the UI
  let names = [...document.querySelectorAll("#service-workers .target-name")];
  names = names.map(element => element.textContent);
  ok(names.length == 0, "All service workers were removed from the list.");

  yield removeTab(swTab);
  yield closeAboutDebugging(tab);
}

add_task(function* () {
  yield testBody(FETCH_SW_TAB_URL, "Listening for fetch events.");
  yield testBody(EMPTY_SW_TAB_URL, "Not listening for fetch events.");
});
