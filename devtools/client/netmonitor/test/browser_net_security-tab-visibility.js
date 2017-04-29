/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that security details tab is visible only when it should.
 */

add_task(function* () {
  const TEST_DATA = [
    {
      desc: "http request",
      uri: "http://example.com" + CORS_SJS_PATH,
      visibleOnNewEvent: false,
      visibleOnSecurityInfo: false,
      visibleOnceComplete: false,
    }, {
      desc: "working https request",
      uri: "https://example.com" + CORS_SJS_PATH,
      visibleOnNewEvent: false,
      visibleOnSecurityInfo: true,
      visibleOnceComplete: true,
    }, {
      desc: "broken https request",
      uri: "https://nocert.example.com",
      isBroken: true,
      visibleOnNewEvent: false,
      visibleOnSecurityInfo: true,
      visibleOnceComplete: true,
    }
  ];

  let { tab, monitor } = yield initNetMonitor(CUSTOM_GET_URL);
  let { document, store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  let { getSelectedRequest } =
    windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

  for (let testcase of TEST_DATA) {
    info("Testing Security tab visibility for " + testcase.desc);
    let onNewItem = monitor.panelWin.once(EVENTS.NETWORK_EVENT);
    let onSecurityInfo = monitor.panelWin.once(EVENTS.RECEIVED_SECURITY_INFO);
    let onComplete = testcase.isBroken ?
                       waitForSecurityBrokenNetworkEvent() :
                       waitForNetworkEvents(monitor, 1);

    info("Performing a request to " + testcase.uri);
    yield ContentTask.spawn(tab.linkedBrowser, testcase.uri, function* (url) {
      content.wrappedJSObject.performRequests(1, url);
    });

    info("Waiting for new network event.");
    yield onNewItem;

    info("Selecting the request.");
    EventUtils.sendMouseEvent({ type: "mousedown" },
      document.querySelectorAll(".request-list-item")[0]);

    is(getSelectedRequest(store.getState()).securityState, undefined,
       "Security state has not yet arrived.");
    is(!!document.querySelector("#security-tab"), testcase.visibleOnNewEvent,
      "Security tab is " + (testcase.visibleOnNewEvent ? "visible" : "hidden") +
      " after new request was added to the menu.");

    info("Waiting for security information to arrive.");
    yield onSecurityInfo;

    yield waitUntil(() => !!getSelectedRequest(store.getState()).securityState);
    ok(getSelectedRequest(store.getState()).securityState,
       "Security state arrived.");
    is(!!document.querySelector("#security-tab"), testcase.visibleOnSecurityInfo,
       "Security tab is " + (testcase.visibleOnSecurityInfo ? "visible" : "hidden") +
       " after security information arrived.");

    info("Waiting for request to complete.");
    yield onComplete;

    is(!!document.querySelector("#security-tab"), testcase.visibleOnceComplete,
       "Security tab is " + (testcase.visibleOnceComplete ? "visible" : "hidden") +
       " after request has been completed.");

    info("Clearing requests.");
    store.dispatch(Actions.clearRequests());
  }

  return teardown(monitor);

  /**
   * Returns a promise that's resolved once a request with security issues is
   * completed.
   */
  function waitForSecurityBrokenNetworkEvent() {
    let awaitedEvents = [
      "UPDATING_REQUEST_HEADERS",
      "RECEIVED_REQUEST_HEADERS",
      "UPDATING_REQUEST_COOKIES",
      "RECEIVED_REQUEST_COOKIES",
      "STARTED_RECEIVING_RESPONSE",
      "UPDATING_RESPONSE_CONTENT",
      "RECEIVED_RESPONSE_CONTENT",
      "UPDATING_EVENT_TIMINGS",
      "RECEIVED_EVENT_TIMINGS",
    ];

    let promises = awaitedEvents.map((event) => {
      return monitor.panelWin.once(EVENTS[event]);
    });

    return Promise.all(promises);
  }
});
