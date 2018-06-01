/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* global sendAsyncMessage */

"use strict";

// Test that clicking on the Push button next to a Service Worker works as
// intended in about:debugging.
// It should trigger a "push" notification in the worker.

// Service workers can't be loaded from chrome://, but http:// is ok with
// dom.serviceWorkers.testing.enabled turned on.
const SERVICE_WORKER = URL_ROOT + "service-workers/push-sw.js";
const TAB_URL = URL_ROOT + "service-workers/push-sw.html";

add_task(async function() {
  await enableServiceWorkerDebugging();
  const { tab, document } = await openAboutDebugging("workers");

  // Listen for mutations in the service-workers list.
  const serviceWorkersElement = getServiceWorkerList(document);

  // Open a tab that registers a push service worker.
  const swTab = await addTab(TAB_URL);

  info("Make the test page notify us when the service worker sends a message.");

  await ContentTask.spawn(swTab.linkedBrowser, {}, function() {
    const win = content.wrappedJSObject;
    win.navigator.serviceWorker.addEventListener("message", function(event) {
      sendAsyncMessage(event.data);
    });
  });

  // Expect the service worker to claim the test window when activating.
  const onClaimed = onTabMessage(swTab, "sw-claimed");

  info("Wait until the service worker appears in the UI");
  await waitUntilServiceWorkerContainer(SERVICE_WORKER, document);

  info("Ensure that the registration resolved before trying to interact with " +
    "the service worker.");
  await waitForServiceWorkerRegistered(swTab);
  ok(true, "Service worker registration resolved");

  await waitForServiceWorkerActivation(SERVICE_WORKER, document);

  // Retrieve the Push button for the worker.
  const names = [...document.querySelectorAll("#service-workers .target-name")];
  const name = names.filter(element => element.textContent === SERVICE_WORKER)[0];
  ok(name, "Found the service worker in the list");

  const targetElement = name.parentNode.parentNode;

  const pushBtn = targetElement.querySelector(".push-button");
  ok(pushBtn, "Found its push button");

  info("Wait for the service worker to claim the test window before proceeding.");
  await onClaimed;

  info("Click on the Push button and wait for the service worker to receive " +
    "a push notification");
  const onPushNotification = onTabMessage(swTab, "sw-pushed");

  pushBtn.click();
  await onPushNotification;
  ok(true, "Service worker received a push notification");

  // Finally, unregister the service worker itself.
  try {
    await unregisterServiceWorker(swTab, serviceWorkersElement);
    ok(true, "Service worker registration unregistered");
  } catch (e) {
    ok(false, "SW not unregistered; " + e);
  }

  await removeTab(swTab);
  await closeAboutDebugging(tab);
});

/**
 * Helper to listen once on a message sent using postMessage from the provided tab.
 */
function onTabMessage(tab, message) {
  const mm = tab.linkedBrowser.messageManager;
  return new Promise(done => {
    mm.addMessageListener(message, function listener() {
      mm.removeMessageListener(message, listener);
      done();
    });
  });
}
