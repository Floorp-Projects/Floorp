/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that Security details tab shows an error message with broken connections.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(CUSTOM_GET_URL);
  let { document, EVENTS, NetMonitorView } = monitor.panelWin;

  NetMonitorView.RequestsMenu.lazyUpdate = false;

  info("Requesting a resource that has a certificate problem.");

  let wait = waitForSecurityBrokenNetworkEvent();
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests(1, "https://nocert.example.com");
  });
  yield wait;

  wait = waitForDOM(document, "#panel-5");
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelector("#details-pane-toggle"));
  document.querySelector("#tab-5 a").click();
  yield wait;

  let errormsg = document.querySelector(".security-info-value");
  isnot(errormsg.textContent, "", "Error message is not empty.");

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
