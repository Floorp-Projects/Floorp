/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Bug 1360457 - Mis-alignment between headers and columns on overflow
 */

add_task(async function() {
  requestLongerTimeout(4);

  const { tab, monitor } = await initNetMonitor(INFINITE_GET_URL, {
    enableCache: true,
    requestCount: 1,
  });
  const { document, windowRequire, store } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  // Wait until the first request makes the empty notice disappear
  await waitForRequestListToAppear();

  const requestsContainerScroll = document.querySelector(
    ".requests-list-scroll"
  );
  ok(requestsContainerScroll, "Container element exists as expected.");
  const requestsContainer = document.querySelector(".requests-list-row-group");
  const headers = document.querySelector(".requests-list-headers");
  ok(headers, "Headers element exists as expected.");

  await waitForRequestsToOverflowContainer(monitor, requestsContainerScroll);

  testColumnsAlignment(headers, requestsContainer);

  // Stop doing requests.
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    content.wrappedJSObject.stopRequests();
  });

  // Done: clean up.
  return teardown(monitor);

  function waitForRequestListToAppear() {
    info(
      "Waiting until the empty notice disappears and is replaced with the list"
    );
    return waitUntil(
      () => !!document.querySelector(".requests-list-row-group")
    );
  }
});

async function waitForRequestsToOverflowContainer(monitor, requestList) {
  info("Waiting for enough requests to overflow the container");
  while (true) {
    info("Waiting for one network request");
    await waitForNetworkEvents(monitor, 1);
    if (requestList.scrollHeight > requestList.clientHeight + 50) {
      info("The list is long enough, returning");
      return;
    }
  }
}
