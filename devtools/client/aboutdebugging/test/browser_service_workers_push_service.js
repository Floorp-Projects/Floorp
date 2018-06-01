/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that a Service Worker registration's Push Service subscription appears
// in about:debugging if it exists, and disappears when unregistered.

// Service workers can't be loaded from chrome://, but http:// is ok with
// dom.serviceWorkers.testing.enabled turned on.
const SERVICE_WORKER = URL_ROOT + "service-workers/push-sw.js";
const TAB_URL = URL_ROOT + "service-workers/push-sw.html";

const FAKE_ENDPOINT = "https://fake/endpoint";

const PushService = Cc["@mozilla.org/push/Service;1"]
  .getService(Ci.nsIPushService).wrappedJSObject;

add_task(async function() {
  info("Turn on workers via mochitest http.");
  await enableServiceWorkerDebugging();
  // Enable the push service.
  await pushPref("dom.push.connection.enabled", true);

  info("Mock the push service");
  PushService.service = {
    _registrations: new Map(),
    _notify(scope) {
      Services.obs.notifyObservers(
        null,
        PushService.subscriptionModifiedTopic,
        scope);
    },
    init() {},
    register(pageRecord) {
      const registration = {
        endpoint: FAKE_ENDPOINT
      };
      this._registrations.set(pageRecord.scope, registration);
      this._notify(pageRecord.scope);
      return Promise.resolve(registration);
    },
    registration(pageRecord) {
      return Promise.resolve(this._registrations.get(pageRecord.scope));
    },
    unregister(pageRecord) {
      const deleted = this._registrations.delete(pageRecord.scope);
      if (deleted) {
        this._notify(pageRecord.scope);
      }
      return Promise.resolve(deleted);
    },
  };

  const { tab, document } = await openAboutDebugging("workers");

  // Listen for mutations in the service-workers list.
  const serviceWorkersElement = document.getElementById("service-workers");

  // Open a tab that registers a push service worker.
  const swTab = await addTab(TAB_URL);

  info("Wait until the service worker appears in about:debugging");
  await waitUntilServiceWorkerContainer(SERVICE_WORKER, document);

  await waitForServiceWorkerActivation(SERVICE_WORKER, document);

  // Wait for the service worker details to update.
  const names = [...document.querySelectorAll("#service-workers .target-name")];
  const name = names.filter(element => element.textContent === SERVICE_WORKER)[0];
  ok(name, "Found the service worker in the list");

  const targetContainer = name.closest(".target-container");

  // Retrieve the push subscription endpoint URL, and verify it looks good.
  info("Wait for the push URL");
  const pushURL = await waitUntilElement(".service-worker-push-url", targetContainer);

  info("Found the push service URL in the service worker details");
  is(pushURL.textContent, FAKE_ENDPOINT, "The push service URL looks correct");

  // Unsubscribe from the push service.
  ContentTask.spawn(swTab.linkedBrowser, {}, function() {
    const win = content.wrappedJSObject;
    return win.sub.unsubscribe();
  });

  // Wait for the service worker details to update again
  info("Wait until the push URL is removed from the UI");
  await waitUntil(() => !targetContainer.querySelector(".service-worker-push-url"), 100);
  info("The push service URL should be removed");

  // Finally, unregister the service worker itself.
  try {
    await unregisterServiceWorker(swTab, serviceWorkersElement);
    ok(true, "Service worker registration unregistered");
  } catch (e) {
    ok(false, "SW not unregistered; " + e);
  }

  info("Unmock the push service");
  PushService.service = null;

  await removeTab(swTab);
  await closeAboutDebugging(tab);
});
