/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Bug 863102 - Automatically scroll down upon new network requests.
 */
add_task(function* () {
  requestLongerTimeout(2);

  let [, , monitor] = yield initNetMonitor(INFINITE_GET_URL);
  let win = monitor.panelWin;
  let topNode = win.document.getElementById("requests-menu-contents");
  let requestsContainer = topNode.getElementsByTagName("scrollbox")[0];
  ok(!!requestsContainer, "Container element exists as expected.");

  // (1) Check that the scroll position is maintained at the bottom
  // when the requests overflow the vertical size of the container.
  yield waitForRequestsToOverflowContainer();
  yield waitForScroll();
  ok(scrolledToBottom(requestsContainer), "Scrolled to bottom on overflow.");

  // (2) Now set the scroll position somewhere in the middle and check
  // that additional requests do not change the scroll position.
  let children = requestsContainer.childNodes;
  let middleNode = children.item(children.length / 2);
  middleNode.scrollIntoView();
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
  ok(scrolledToBottom(requestsContainer), "Still scrolled to bottom.");

  // (4) Now select an item in the list and check that additional requests
  // do not change the scroll position.
  monitor.panelWin.NetMonitorView.RequestsMenu.selectedIndex = 0;
  yield waitForNetworkEvents(monitor, 8);
  yield waitSomeTime();
  is(requestsContainer.scrollTop, 0, "Did not scroll.");

  // Done: clean up.
  yield teardown(monitor);

  function* waitForRequestsToOverflowContainer() {
    while (true) {
      yield waitForNetworkEvents(monitor, 1);
      if (requestsContainer.scrollHeight > requestsContainer.clientHeight) {
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
    return monitor._view.RequestsMenu.widget.once("scroll-to-bottom");
  }
});
