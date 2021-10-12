/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests image caches can be displayed in the network monitor
 */

add_task(async function() {
  const { monitor } = await initNetMonitor(IMAGE_CACHE_URL, {
    enableCache: true,
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const { getDisplayedRequests, getSortedRequests } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  const waitForEvents = waitForNetworkEvents(monitor, 4, {
    expectedEventTimings: 0,
    expectedPayloadReady: 1,
  });

  // Using `BrowserTestUtils.loadURI` instead of `navigateTo` because
  // `navigateTo` would use `gBrowser.reloadTab` to reload the tab
  // when the current uri is the same as the one that is going to navigate.
  // And the issue is that with `gBrowser.reloadTab`, `VALIDATE_ALWAYS`
  // flag will be applied to the loadgroup, such that the sub resources
  // are forced to be revalidated. So we use `BrowserTestUtils.loadURI` to
  // avoid having `VALIDATE_ALWAYS` applied.
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, IMAGE_CACHE_URL);
  await waitForEvents;

  const requests = document.querySelectorAll(".request-list-item");
  is(requests.length, 4, "There should be 4 requests");

  const requestData = {
    uri: EXAMPLE_URL + "test-image.png",
    details: {
      status: 200,
      statusText: "OK (cached)",
      displayedStatus: "cached",
      type: "png",
      fullMimeType: "image/png",
    },
  };

  for (let i = 1; i < requests.length; ++i) {
    const request = requests[i];

    // mouseover is needed for tooltips to show up.
    const requestsListStatus = request.querySelector(".status-code");
    EventUtils.sendMouseEvent({ type: "mouseover" }, requestsListStatus);
    await waitUntil(() => requestsListStatus.title);

    await verifyRequestItemTarget(
      document,
      getDisplayedRequests(store.getState()),
      getSortedRequests(store.getState())[i],
      "GET",
      requestData.uri,
      requestData.details
    );
  }
  await teardown(monitor);
});
