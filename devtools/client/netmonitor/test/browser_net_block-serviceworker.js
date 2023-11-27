/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Service workers only work on https
const URL = EXAMPLE_URL.replace("http:", "https:");

const TEST_URL = URL + "service-workers/status-codes.html";

// Test that request blocking works for service worker requests.
add_task(async function () {
  const { tab, monitor } = await initNetMonitor(TEST_URL, {
    enableCache: true,
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  info("Registering the service worker...");
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    await content.wrappedJSObject.registerServiceWorker();
  });

  info("Performing the service worker request");
  await performRequests(monitor, tab, 1);

  info("Open the request blocking panel and block service-workers request");
  store.dispatch(Actions.toggleRequestBlockingPanel());

  info("Block the image request from the service worker");
  await addBlockedRequest("service-workers/test-image.png", monitor);

  await clearNetworkEvents(monitor);

  info("Performing the service worker request again");
  await performRequests(monitor, tab, 1);

  // Wait till there are four resources rendered in the results
  await waitForDOMIfNeeded(document, ".request-list-item", 4);

  const requestItems = document.querySelectorAll(".request-list-item");
  ok(
    !checkRequestListItemBlocked(requestItems[0]),
    "The first service worker request was not blocked"
  );
  ok(
    checkRequestListItemBlocked(requestItems[requestItems.length - 1]),
    "The last service worker request was blocked"
  );

  info("Unregistering the service worker...");
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    await content.wrappedJSObject.unregisterServiceWorker();
  });

  await teardown(monitor);
});
