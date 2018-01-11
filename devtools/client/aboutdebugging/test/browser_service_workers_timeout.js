/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

// Service workers can't be loaded from chrome://,
// but http:// is ok with dom.serviceWorkers.testing.enabled turned on.
const SERVICE_WORKER = URL_ROOT + "service-workers/empty-sw.js";
const TAB_URL = URL_ROOT + "service-workers/empty-sw.html";

const SW_TIMEOUT = 1000;

add_task(function* () {
  yield enableServiceWorkerDebugging();
  yield pushPref("dom.serviceWorkers.idle_timeout", SW_TIMEOUT);
  yield pushPref("dom.serviceWorkers.idle_extended_timeout", SW_TIMEOUT);

  let { tab, document } = yield openAboutDebugging("workers");

  let serviceWorkersElement = getServiceWorkerList(document);

  let swTab = yield addTab(TAB_URL);

  info("Wait until the service worker appears in about:debugging");
  yield waitUntilServiceWorkerContainer(SERVICE_WORKER, document);

  // Ensure that the registration resolved before trying to connect to the sw
  yield waitForServiceWorkerRegistered(swTab);
  ok(true, "Service worker registration resolved");

  // Retrieve the DEBUG button for the worker
  let names = [...document.querySelectorAll("#service-workers .target-name")];
  let name = names.filter(element => element.textContent === SERVICE_WORKER)[0];
  ok(name, "Found the service worker in the list");
  let targetElement = name.parentNode.parentNode;
  let debugBtn = targetElement.querySelector(".debug-button");
  ok(debugBtn, "Found its debug button");

  // Click on it and wait for the toolbox to be ready
  let onToolboxReady = new Promise(done => {
    gDevTools.once("toolbox-ready", function (e, toolbox) {
      done(toolbox);
    });
  });
  debugBtn.click();

  let toolbox = yield onToolboxReady;

  // Wait for more than the regular timeout,
  // so that if the worker freezing doesn't work,
  // it will be destroyed and removed from the list
  yield new Promise(done => {
    setTimeout(done, SW_TIMEOUT * 2);
  });

  assertHasTarget(true, document, "service-workers", SERVICE_WORKER);
  ok(targetElement.querySelector(".debug-button"),
    "The debug button is still there");

  yield toolbox.destroy();
  toolbox = null;

  // Now ensure that the worker is correctly destroyed
  // after we destroy the toolbox.
  // The DEBUG button should disappear once the worker is destroyed.
  info("Wait until the debug button disappears");
  yield waitUntil(() => {
    return !targetElement.querySelector(".debug-button");
  });

  // Finally, unregister the service worker itself.
  try {
    yield unregisterServiceWorker(swTab, serviceWorkersElement);
    ok(true, "Service worker registration unregistered");
  } catch (e) {
    ok(false, "SW not unregistered; " + e);
  }

  assertHasTarget(false, document, "service-workers", SERVICE_WORKER);

  yield removeTab(swTab);
  yield closeAboutDebugging(tab);
});
