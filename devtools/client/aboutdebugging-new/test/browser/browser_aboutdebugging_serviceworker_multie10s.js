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
const PROCESS_COUNT_PREF = "dom.ipc.processCount";
const OPTOUT_PREF = "dom.ipc.multiOptOut";

// Test that debugging service workers is disabled when we are in multi e10s
// (enabling/disabling multi e10s via process count)
add_task(async function() {
  const enableFn = async () => pushPref(PROCESS_COUNT_PREF, 8);
  const disableFn = async () => pushPref(PROCESS_COUNT_PREF, 1);

  await testDebuggingSW(enableFn, disableFn);
});

// Test that debugging service workers is disabled when we are in multi e10s
// (enabling/disabling multi e10s via opt out pref)
add_task(async function() {
  const enableFn = async () => pushPref(OPTOUT_PREF, 0);
  const disableFn = async () => pushPref(OPTOUT_PREF, 1);

  await testDebuggingSW(enableFn, disableFn);
});

async function testDebuggingSW(enableMultiE10sFn, disableMultiE10sFn) {
  // enable service workers
  await pushPref("dom.serviceWorkers.testing.enabled", true);

  const { document, tab, window } = await openAboutDebugging({
    enableWorkerUpdates: true,
  });

  // If the test starts too quickly, the test will timeout on some platforms.
  // See Bug 1533111.
  await wait(1000);

  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  // disable multi e10s
  info("Disabling multi e10s");
  await disableMultiE10sFn();

  // Open a tab that registers a push service worker.
  const swTab = await addTab(TAB_URL);

  info("Forward service worker messages to the test");
  await forwardServiceWorkerMessage(swTab);

  info(
    "Wait for the service worker to claim the test window before proceeding."
  );
  await onTabMessage(swTab, "sw-claimed");

  info("Wait until the service worker appears and is running");
  await waitUntil(() => {
    const target = findDebugTargetByText(SERVICE_WORKER, document);
    const status = target && target.querySelector(".qa-worker-status");
    return status && status.textContent === "Running";
  });

  let targetElement = findDebugTargetByText(SERVICE_WORKER, document);
  let pushButton = targetElement.querySelector(".qa-push-button");
  ok(!pushButton.disabled, "Push button is not disabled");
  ok(!pushButton.hasAttribute("title"), "Push button has no title attribute");

  let inspectButton = targetElement.querySelector(
    ".qa-debug-target-inspect-button"
  );
  ok(!inspectButton.disabled, "Inspect button is not disabled");
  ok(
    !inspectButton.hasAttribute("title"),
    "Inspect button has no title attribute"
  );

  // enable multi e10s
  info("Enabling multi e10s");
  await enableMultiE10sFn();

  info("Wait for debug target to re-render");
  await waitUntil(() => {
    targetElement = findDebugTargetByText(SERVICE_WORKER, document);
    pushButton = targetElement.querySelector(".qa-push-button");
    return pushButton.disabled;
  });

  ok(pushButton.disabled, "Push button is disabled");
  inspectButton = targetElement.querySelector(
    ".qa-debug-target-inspect-button"
  );
  ok(inspectButton.disabled, "Inspect button is disabled");
  ok(inspectButton.hasAttribute("title"), "Button has a title attribute");

  checkButtonTitle(
    inspectButton,
    "Service Worker inspection is currently disabled"
  );
  checkButtonTitle(pushButton, "Service Worker push is currently disabled");

  info("Unregister the service worker");
  await unregisterServiceWorker(swTab, "pushServiceWorkerRegistration");

  info("Wait until the service worker disappears from about:debugging");
  await waitUntil(() => !findDebugTargetByText(SERVICE_WORKER, document));

  info("Remove browser tabs");
  await removeTab(swTab);
  await removeTab(tab);
}

function checkButtonTitle(button, expectedTitle) {
  const title = button.getAttribute("title");
  ok(title.includes(expectedTitle), "Button has the expected title");
}
