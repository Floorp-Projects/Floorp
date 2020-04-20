/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Bug 863102 - Automatically scroll down upon new network requests.
 * edited to account for changes made to fix Bug 1360457
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

  const requestsContainer = document.querySelector(".requests-list-scroll");
  ok(requestsContainer, "Container element exists as expected.");

  // (1) Check that the scroll position is maintained at the bottom
  // when the requests overflow the vertical size of the container.
  await waitForRequestsToOverflowContainer();
  await waitForScroll();
  ok(true, "Scrolled to bottom on overflow.");

  // (2) Now scroll to the top and check that additional requests
  // do not change the scroll position.
  requestsContainer.scrollTop = 0;
  await waitSomeTime();
  ok(!scrolledToBottom(requestsContainer), "Not scrolled to bottom.");
  // save for comparison later
  const { scrollTop } = requestsContainer;
  await waitForNetworkEvents(monitor, 8);
  await waitSomeTime();
  is(requestsContainer.scrollTop, scrollTop, "Did not scroll.");

  // (3) Now set the scroll position back at the bottom and check that
  // additional requests *do* cause the container to scroll down.
  requestsContainer.scrollTop = requestsContainer.scrollHeight;
  ok(scrolledToBottom(requestsContainer), "Set scroll position to bottom.");
  await waitForNetworkEvents(monitor, 8);
  await waitForScroll();
  ok(true, "Still scrolled to bottom.");

  // (4) Now select the first item in the list
  // and check that additional requests do not change the scroll position
  // from just below the headers.
  store.dispatch(Actions.selectRequestByIndex(0));
  await waitForNetworkEvents(monitor, 8);
  await waitSomeTime();
  const requestsContainerHeaders = document.querySelector(
    ".requests-list-headers"
  );
  const headersHeight = Math.floor(
    requestsContainerHeaders.getBoundingClientRect().height
  );
  is(requestsContainer.scrollTop, headersHeight, "Did not scroll.");

  // Stop doing requests.
  await SpecialPowers.spawn(tab.linkedBrowser, [], function() {
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

  async function waitForRequestsToOverflowContainer() {
    info("Waiting for enough requests to overflow the container");
    while (true) {
      info("Waiting for one network request");
      await waitForNetworkEvents(monitor, 1);
      if (
        requestsContainer.scrollHeight >
        requestsContainer.clientHeight + 50
      ) {
        info("The list is long enough, returning");
        return;
      }
    }
  }

  function scrolledToBottom(element) {
    return element.scrollTop + element.clientHeight >= element.scrollHeight;
  }

  function waitSomeTime() {
    // Wait to make sure no scrolls happen
    return wait(50);
  }

  function waitForScroll() {
    info("Waiting for the list to scroll to bottom");
    return waitUntil(() => scrolledToBottom(requestsContainer));
  }
});
