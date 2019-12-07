/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-serviceworker.js */
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "helper-serviceworker.js",
  this
);

const SW_TAB_URL = URL_ROOT + "resources/service-workers/controlled-sw.html";
const SW_URL = URL_ROOT + "resources/service-workers/controlled-sw.js";

/**
 * Test that the service worker has the status "registering" when the service worker is
 * not installed yet. Other states (stopped, running) are covered by the existing tests.
 */
add_task(async function() {
  await enableServiceWorkerDebugging();

  const { document, tab, window } = await openAboutDebugging({
    enableWorkerUpdates: true,
  });
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  info("Open tab with a service worker that never leaves `registering` status");
  const swTab = await addTab(SW_TAB_URL);

  // Wait for the registration to make sure service worker has been started, and that we
  // are not just reading STOPPED as the initial state.
  await waitForRegistration(swTab);

  info("Wait until the service worker is in registering status");
  await waitForServiceWorkerRegistering(SW_URL, document);

  // Check that the buttons are displayed as expected.
  checkButtons(
    { inspect: true, push: false, start: false, unregister: false },
    SW_URL,
    document
  );

  info("Install the service worker");
  ContentTask.spawn(swTab.linkedBrowser, {}, () =>
    content.wrappedJSObject.installServiceWorker()
  );

  info("Wait until the service worker is running");
  await waitForServiceWorkerRunning(SW_URL, document);

  checkButtons(
    { inspect: true, push: true, start: false, unregister: true },
    SW_URL,
    document
  );

  info("Unregister service worker");
  await unregisterServiceWorker(swTab);

  info("Wait until the service worker disappears from about:debugging");
  await waitUntil(() => !findDebugTargetByText(SW_URL, document));

  info("Remove tabs");
  await removeTab(swTab);
  await removeTab(tab);
});

function checkButtons(
  { inspect, push, start, unregister },
  workerText,
  document
) {
  const targetElement = findDebugTargetByText(SW_URL, document);

  const inspectButton = targetElement.querySelector(
    ".qa-debug-target-inspect-button"
  );
  const pushButton = targetElement.querySelector(".qa-push-button");
  const startButton = targetElement.querySelector(".qa-start-button");
  const unregisterButton = targetElement.querySelector(".qa-unregister-button");

  is(
    !!inspectButton,
    inspect,
    "Inspect button should be " + (inspect ? "visible" : "hidden")
  );
  is(
    !!pushButton,
    push,
    "Push button should be " + (push ? "visible" : "hidden")
  );
  is(
    !!startButton,
    start,
    "Start button should be " + (start ? "visible" : "hidden")
  );
  is(
    !!unregisterButton,
    unregister,
    "Unregister button should be " + (unregister ? "visible" : "hidden")
  );
}
