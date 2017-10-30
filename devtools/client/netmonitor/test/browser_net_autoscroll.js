/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Bug 863102 - Automatically scroll down upon new network requests.
 */
add_task(function* () {
  requestLongerTimeout(4);

  let { monitor } = yield initNetMonitor(INFINITE_GET_URL, true);
  let { document, windowRequire, store } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  // Wait until the first request makes the empty notice disappear
  yield waitForRequestListToAppear();

  let requestsContainer = document.querySelector(".requests-list-contents");
  ok(requestsContainer, "Container element exists as expected.");

  // (1) Check that the scroll position is maintained at the bottom
  // when the requests overflow the vertical size of the container.
  yield waitForRequestsToOverflowContainer();
  yield waitForScroll();
  ok(true, "Scrolled to bottom on overflow.");

  // (2) Now set the scroll position to the first item and check
  // that additional requests do not change the scroll position.
  let firstNode = requestsContainer.firstChild;
  firstNode.scrollIntoView();
  yield waitSomeTime();
  ok(!scrolledToBottom(requestsContainer), "Not scrolled to bottom.");
  // save for comparison later
  let scrollTop = requestsContainer.scrollTop;
  yield waitForNetworkEvents(monitor, 8);
  yield waitSomeTime();
  is(requestsContainer.scrollTop, scrollTop, "Did not scroll.");

  // (3) Now set the scroll position back at the bottom and check that
  // additional requests *do* cause the container to scroll down.
  requestsContainer.scrollTop = requestsContainer.scrollHeight;
  ok(scrolledToBottom(requestsContainer), "Set scroll position to bottom.");
  yield waitForNetworkEvents(monitor, 8);
  yield waitForScroll();
  ok(true, "Still scrolled to bottom.");

  // (4) Now select an item in the list and check that additional requests
  // do not change the scroll position.
  store.dispatch(Actions.selectRequestByIndex(0));
  yield waitForNetworkEvents(monitor, 8);
  yield waitSomeTime();
  is(requestsContainer.scrollTop, 0, "Did not scroll.");

  // Done: clean up.
  return teardown(monitor);

  function waitForRequestListToAppear() {
    info("Waiting until the empty notice disappears and is replaced with the list");
    return waitUntil(() => !!document.querySelector(".requests-list-contents"));
  }

  function* waitForRequestsToOverflowContainer() {
    info("Waiting for enough requests to overflow the container");
    while (true) {
      info("Waiting for one network request");
      yield waitForNetworkEvents(monitor, 1);
      console.log(requestsContainer.scrollHeight);
      console.log(requestsContainer.clientHeight);
      if (requestsContainer.scrollHeight > requestsContainer.clientHeight) {
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
