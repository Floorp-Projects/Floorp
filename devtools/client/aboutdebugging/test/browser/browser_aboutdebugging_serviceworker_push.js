/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-serviceworker.js */
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "helper-serviceworker.js",
  this
);

const SERVICE_WORKER = URL_ROOT + "resources/service-workers/push-sw.js";
const TAB_URL = URL_ROOT + "resources/service-workers/push-sw.html";

// Test that clicking on the Push button next to a Service Worker works as intended.
// It should trigger a "push" notification in the worker.
add_task(async function() {
  await enableServiceWorkerDebugging();
  const { document, tab, window } = await openAboutDebugging({
    enableWorkerUpdates: true,
  });
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  // Open a tab that registers a push service worker.
  const swTab = await addTab(TAB_URL);

  info(
    "Wait for the service worker to claim the test window before proceeding."
  );
  await SpecialPowers.spawn(
    swTab.linkedBrowser,
    [],
    () => content.wrappedJSObject.onSwClaimed
  );

  info("Wait until the service worker appears and is running");
  const targetElement = await waitForServiceWorkerRunning(
    SERVICE_WORKER,
    document
  );

  // Retrieve the Push button for the worker.
  const pushButton = targetElement.querySelector(".qa-push-button");
  ok(pushButton, "Found its push button");

  info("Click on the Push button and wait for the push notification");
  const onPushNotification = onServiceWorkerMessage(swTab, "sw-pushed");
  pushButton.click();
  await onPushNotification;

  info("Unregister the service worker");
  await unregisterServiceWorker(swTab);

  info("Wait until the service worker disappears from about:debugging");
  await waitUntil(() => !findDebugTargetByText(SERVICE_WORKER, document));

  info("Remove the service worker tab");
  await removeTab(swTab);

  await removeTab(tab);
});
