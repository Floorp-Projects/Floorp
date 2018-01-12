/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Bug 1360457 - Mis-alignment between headers and columns on overflow
 */

add_task(async function () {
  requestLongerTimeout(4);

  let { tab, monitor } = await initNetMonitor(INFINITE_GET_URL, true);
  let { document, windowRequire, store } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  // Wait until the first request makes the empty notice disappear
  await waitForRequestListToAppear();

  let requestsContainer = document.querySelector(".requests-list-contents");
  ok(requestsContainer, "Container element exists as expected.");
  let headers = document.querySelector(".requests-list-headers");
  ok(headers, "Headers element exists as expected.");

  await waitForRequestsToOverflowContainer();

  testColumnsAlignment(headers, requestsContainer);

  // Stop doing requests.
  await ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.stopRequests();
  });

  // Done: clean up.
  return teardown(monitor);

  function waitForRequestListToAppear() {
    info("Waiting until the empty notice disappears and is replaced with the list");
    return waitUntil(() => !!document.querySelector(".requests-list-contents"));
  }
});

async function waitForRequestsToOverflowContainer(monitor, requestList) {
  info("Waiting for enough requests to overflow the container");
  while (true) {
    info("Waiting for one network request");
    await waitForNetworkEvents(monitor, 1);
    if (requestList.scrollHeight > requestList.clientHeight) {
      info("The list is long enough, returning");
      return;
    }
  }
}
