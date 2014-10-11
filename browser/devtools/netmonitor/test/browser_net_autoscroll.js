/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

///////////////////
//
// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejection should be fixed.
//
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("TypeError: aValue.content is undefined");

/**
 * Bug 863102 - Automatically scroll down upon new network requests.
 */

function test() {
  requestLongerTimeout(2);
  let monitor, debuggee, requestsContainer, scrollTop;

  initNetMonitor(INFINITE_GET_URL).then(([aTab, aDebuggee, aMonitor]) => {
    monitor = aMonitor;
    debuggee = aDebuggee;
    let win = monitor.panelWin;
    let topNode = win.document.getElementById("requests-menu-contents");
    requestsContainer = topNode.getElementsByTagName("scrollbox")[0];
    ok(!!requestsContainer, "Container element exists as expected.");
  })

  // (1) Check that the scroll position is maintained at the bottom
  // when the requests overflow the vertical size of the container.
  .then(() => {
    return waitForRequestsToOverflowContainer(monitor, requestsContainer);
  })
  .then(() => {
    ok(scrolledToBottom(requestsContainer), "Scrolled to bottom on overflow.");
  })

  // (2) Now set the scroll position somewhere in the middle and check
  // that additional requests do not change the scroll position.
  .then(() => {
    let children = requestsContainer.childNodes;
    let middleNode = children.item(children.length / 2);
    middleNode.scrollIntoView();
    ok(!scrolledToBottom(requestsContainer), "Not scrolled to bottom.");
    scrollTop = requestsContainer.scrollTop; // save for comparison later
    return waitForNetworkEvents(monitor, 8);
  })
  .then(() => {
    is(requestsContainer.scrollTop, scrollTop, "Did not scroll.");
  })

  // (3) Now set the scroll position back at the bottom and check that
  // additional requests *do* cause the container to scroll down.
  .then(() => {
    requestsContainer.scrollTop = requestsContainer.scrollHeight;
    ok(scrolledToBottom(requestsContainer), "Set scroll position to bottom.");
    return waitForNetworkEvents(monitor, 8);
  })
  .then(() => {
    ok(scrolledToBottom(requestsContainer), "Still scrolled to bottom.");
  })

  // (4) Now select an item in the list and check that additional requests
  // do not change the scroll position.
  .then(() => {
    monitor.panelWin.NetMonitorView.RequestsMenu.selectedIndex = 0;
    return waitForNetworkEvents(monitor, 8);
  })
  .then(() => {
    is(requestsContainer.scrollTop, 0, "Did not scroll.");
  })

  // Done; clean up.
  .then(() => {
    return teardown(monitor).then(finish);
  })

  // Handle exceptions in the chain of promises.
  .then(null, (err) => {
    ok(false, err);
    finish();
  });

  function waitForRequestsToOverflowContainer (aMonitor, aContainer) {
    return waitForNetworkEvents(aMonitor, 1).then(() => {
      if (aContainer.scrollHeight > aContainer.clientHeight) {
        // Wait for some more just for good measure.
        return waitForNetworkEvents(aMonitor, 8);
      } else {
        return waitForRequestsToOverflowContainer(aMonitor, aContainer);
      }
    });
  }

  function scrolledToBottom(aElement) {
    return aElement.scrollTop + aElement.clientHeight >= aElement.scrollHeight;
  }
}
