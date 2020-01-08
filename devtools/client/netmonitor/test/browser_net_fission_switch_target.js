/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test switching for the top-level target.

const PARENT_PROCESS_URI = "about:robots";
const PARENT_PROCESS_URI_NETWORK_COUNT = 0;
const CONTENT_PROCESS_URI = CUSTOM_GET_URL;
const CONTENT_PROCESS_URI_NETWORK_COUNT = 1;

add_task(async function() {
  await pushPref("devtools.target-switching.enabled", true);

  // We use about:robots, because this page will run in the parent process.
  // Navigating from about:robots to a regular content page will always trigger a target
  // switch, with or without fission.

  info("Open a page that runs on the content process and the net monitor");
  const { monitor, tab, toolbox } = await initNetMonitor(CONTENT_PROCESS_URI);
  const { store } = monitor.panelWin;

  info("Reload the page to show the network event");
  const waitForReloading = waitForNetworkEvents(
    monitor,
    CONTENT_PROCESS_URI_NETWORK_COUNT
  );
  tab.linkedBrowser.reload();
  await waitForReloading;

  info("Navigate to a page that runs on parent process");
  await navigateTo(PARENT_PROCESS_URI, toolbox, monitor, tab);
  is(
    store.getState().requests.requests.length,
    PARENT_PROCESS_URI_NETWORK_COUNT,
    `Request count of ${PARENT_PROCESS_URI} is correct`
  );

  info("Return to a page that runs on content process again");
  await navigateTo(CONTENT_PROCESS_URI, toolbox, monitor, tab);

  info(`Execute more requests in ${CONTENT_PROCESS_URI}`);
  const currentRequestCount = store.getState().requests.requests.length;
  const additionalRequestCount = 3;
  await performRequests(monitor, tab, additionalRequestCount);
  is(
    store.getState().requests.requests.length - currentRequestCount,
    additionalRequestCount,
    "Additional request count is reflected correctly"
  );

  await teardown(monitor);
});

async function navigateTo(uri, toolbox, monitor, tab) {
  const onSwitched = once(toolbox, "switched-target");
  const onReloaded = once(monitor, "reloaded");
  BrowserTestUtils.loadURI(tab.linkedBrowser, uri);
  await onSwitched;
  await onReloaded;
  ok(true, "All events we expected were fired");
}
