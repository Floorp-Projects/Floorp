/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-serviceworker.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-serviceworker.js", this);

const SERVICE_WORKER = URL_ROOT + "resources/service-workers/push-sw.js";
const TAB_URL = URL_ROOT + "resources/service-workers/push-sw.html";

const FAKE_ENDPOINT = "https://fake/endpoint";

// Test that the push service url is displayed for service workers subscribed to a push
// service.
add_task(async function() {
  await enableServiceWorkerDebugging();

  info("Mock the push service");
  mockPushService(FAKE_ENDPOINT);

  const { document, tab, window } =
    await openAboutDebugging({ enableWorkerUpdates: true });
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  // Open a tab that registers a push service worker.
  const swTab = await addTab(TAB_URL);

  info("Forward service worker messages to the test");
  await forwardServiceWorkerMessage(swTab);

  info("Wait for the service worker to claim the test window before proceeding.");
  await onTabMessage(swTab, "sw-claimed");

  info("Wait until the service worker appears and is running");
  const targetElement = await waitForServiceWorkerRunning(SERVICE_WORKER, document);

  info("Subscribe from the push service");
  ContentTask.spawn(swTab.linkedBrowser, {}, () => {
    content.wrappedJSObject.subscribeToPush();
  });

  info("Wait until the push service appears");
  await waitUntil(() => targetElement.querySelector(".qa-worker-push-service-value"));
  const pushUrl = targetElement.querySelector(".qa-worker-push-service-value");

  ok(!!pushUrl, "Push URL is displayed for the serviceworker");
  is(pushUrl.textContent, FAKE_ENDPOINT, "Push URL shows the expected content");

  info("Unsubscribe from the push service");
  ContentTask.spawn(swTab.linkedBrowser, {}, () => {
    content.wrappedJSObject.unsubscribeToPush();
  });

  info("Wait until the push service disappears");
  await waitUntil(() => !targetElement.querySelector(".qa-worker-push-service-value"));

  info("Unregister the service worker");
  await unregisterServiceWorker(swTab);

  info("Wait until the service worker disappears from about:debugging");
  await waitUntil(() => !findDebugTargetByText(SERVICE_WORKER, document));

  info("Remove the service worker tab");
  await removeTab(swTab);

  await removeTab(tab);
});

function mockPushService(endpoint) {
  const PushService = Cc["@mozilla.org/push/Service;1"]
    .getService(Ci.nsIPushService).wrappedJSObject;

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
        endpoint,
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
}
