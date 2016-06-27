/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Service workers can't be loaded from chrome://,
// but http:// is ok with dom.serviceWorkers.testing.enabled turned on.
const SERVICE_WORKER = URL_ROOT + "service-workers/empty-sw.js";
const TAB_URL = URL_ROOT + "service-workers/empty-sw.html";

const SW_TIMEOUT = 1000;

add_task(function* () {
  yield new Promise(done => {
    let options = {"set": [
      // Accept workers from mochitest's http
      ["dom.serviceWorkers.testing.enabled", true],
      // Reduce the timeout to expose issues when service worker
      // freezing is broken
      ["dom.serviceWorkers.idle_timeout", SW_TIMEOUT],
      ["dom.serviceWorkers.idle_extended_timeout", SW_TIMEOUT],
    ]};
    SpecialPowers.pushPrefEnv(options, done);
  });

  let { tab, document } = yield openAboutDebugging("workers");

  let swTab = yield addTab(TAB_URL);

  let serviceWorkersElement = getServiceWorkerList(document);
  yield waitForMutation(serviceWorkersElement, { childList: true });

  assertHasTarget(true, document, "service-workers", SERVICE_WORKER);

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
  yield waitForMutation(targetElement, { childList: true });
  ok(!targetElement.querySelector(".debug-button"),
    "The debug button was removed when the worker was killed");

  // Finally, unregister the service worker itself.
  try {
    yield unregisterServiceWorker(swTab);
    ok(true, "Service worker registration unregistered");
  } catch (e) {
    ok(false, "SW not unregistered; " + e);
  }

  // Now ensure that the worker registration is correctly removed.
  // The list should update once the registration is destroyed.
  yield waitForMutation(serviceWorkersElement, { childList: true });
  assertHasTarget(false, document, "service-workers", SERVICE_WORKER);

  yield removeTab(swTab);
  yield closeAboutDebugging(tab);
});
