/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that clicking on the unregister link in the Service Worker details works
// as intended in about:debugging.
// It should unregister the service worker, which should trigger an update of
// the displayed list of service workers.

// Service workers can't be loaded from chrome://, but http:// is ok with
// dom.serviceWorkers.testing.enabled turned on.
const SCOPE = URL_ROOT + "service-workers/";
const SERVICE_WORKER = SCOPE + "empty-sw.js";
const TAB_URL = SCOPE + "empty-sw.html";

add_task(function* () {
  yield enableServiceWorkerDebugging();

  let { tab, document } = yield openAboutDebugging("workers");

  // Listen for mutations in the service-workers list.
  let serviceWorkersElement = getServiceWorkerList(document);
  let onMutation = waitForMutation(serviceWorkersElement, { childList: true });

  // Open a tab that registers an empty service worker.
  let swTab = yield addTab(TAB_URL);

  // Wait for the service workers-list to update.
  yield onMutation;

  // Check that the service worker appears in the UI.
  assertHasTarget(true, document, "service-workers", SERVICE_WORKER);

  yield waitForServiceWorkerActivation(SERVICE_WORKER, document);

  info("Ensure that the registration resolved before trying to interact with " +
    "the service worker.");
  yield waitForServiceWorkerRegistered(swTab);
  ok(true, "Service worker registration resolved");

  let targets = document.querySelectorAll("#service-workers .target");
  is(targets.length, 1, "One service worker is now displayed.");

  let target = targets[0];
  let name = target.querySelector(".target-name");
  is(name.textContent, SERVICE_WORKER, "Found the service worker in the list");

  info("Check the scope displayed scope is correct");
  let scope = target.querySelector(".service-worker-scope");
  is(scope.textContent, SCOPE,
    "The expected scope is displayed in the service worker info.");

  info("Unregister the service worker via the unregister link.");
  let unregisterLink = target.querySelector(".unregister-link");
  ok(unregisterLink, "Found the unregister link");

  onMutation = waitForMutation(serviceWorkersElement, { childList: true });
  unregisterLink.click();
  yield onMutation;

  is(document.querySelector("#service-workers .target"), null,
   "No service worker displayed anymore.");

  yield removeTab(swTab);
  yield closeAboutDebugging(tab);
});
