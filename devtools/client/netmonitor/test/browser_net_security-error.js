/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Test that Security details tab shows an error message with broken connections.
 */

add_task(function* () {
  let [tab, , monitor] = yield initNetMonitor(CUSTOM_GET_URL);
  let { $, EVENTS, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu, NetworkDetails } = NetMonitorView;
  RequestsMenu.lazyUpdate = false;

  info("Requesting a resource that has a certificate problem.");

  let wait = waitForSecurityBrokenNetworkEvent();
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests(1, "https://nocert.example.com");
  });
  yield wait;

  info("Selecting the request.");
  RequestsMenu.selectedIndex = 0;

  info("Waiting for details pane to be updated.");
  yield monitor.panelWin.once(EVENTS.TAB_UPDATED);

  info("Selecting security tab.");
  NetworkDetails.widget.selectedIndex = 5;

  info("Waiting for security tab to be updated.");
  yield monitor.panelWin.once(EVENTS.TAB_UPDATED);

  let errorbox = $("#security-error");
  let errormsg = $("#security-error-message");
  let infobox = $("#security-information");

  is(errorbox.hidden, false, "Error box is visble.");
  is(infobox.hidden, true, "Information box is hidden.");

  isnot(errormsg.value, "", "Error message is not empty.");

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
