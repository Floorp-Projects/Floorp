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
  let { document, EVENTS, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

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
    RequestsMenu.selectedIndex = 0;

    is(RequestsMenu.selectedItem.securityState, undefined,
       "Security state has not yet arrived.");
    is(!!document.querySelector("#tab-5"), testcase.visibleOnNewEvent,
      "Security tab is " + (testcase.visibleOnNewEvent ? "visible" : "hidden") +
      " after new request was added to the menu.");
    is(!!document.querySelector("#panel-5"), testcase.visibleOnNewEvent,
      "Security panel is " + (testcase.visibleOnNewEvent ? "visible" : "hidden") +
      " after new request was added to the menu.");

    info("Waiting for security information to arrive.");
    yield onSecurityInfo;

    ok(RequestsMenu.selectedItem.securityState,
       "Security state arrived.");
    is(!!document.querySelector("#tab-5"), testcase.visibleOnSecurityInfo,
       "Security tab is " + (testcase.visibleOnSecurityInfo ? "visible" : "hidden") +
       " after security information arrived.");
    is(!!document.querySelector("#panel-5"), testcase.visibleOnSecurityInfo,
      "Security panel is " + (testcase.visibleOnSecurityInfo? "visible" : "hidden") +
      " after security information arrived.");

    info("Waiting for request to complete.");
    yield onComplete;

    is(!!document.querySelector("#tab-5"), testcase.visibleOnceComplete,
       "Security tab is " + (testcase.visibleOnceComplete ? "visible" : "hidden") +
       " after request has been completed.");
    is(!!document.querySelector("#panel-5"), testcase.visibleOnceComplete,
      "Security panel is " + (testcase.visibleOnceComplete? "visible" : "hidden") +
      " after request has been completed.");

    info("Clearing requests.");
    RequestsMenu.clear();
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
