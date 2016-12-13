/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if showing raw headers works.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(POST_DATA_URL);
  info("Starting test... ");

  let { document, EVENTS, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  let wait = waitForNetworkEvents(monitor, 0, 2);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  let origItem = RequestsMenu.getItemAtIndex(0);

  let onTabEvent = monitor.panelWin.once(EVENTS.TAB_UPDATED);
  RequestsMenu.selectedItem = origItem;
  yield onTabEvent;

  EventUtils.sendMouseEvent({ type: "click" },
    document.getElementById("toggle-raw-headers"));

  testShowRawHeaders(origItem);

  EventUtils.sendMouseEvent({ type: "click" },
    document.getElementById("toggle-raw-headers"));

  testHideRawHeaders(document);

  return teardown(monitor);

  /*
   * Tests that raw headers were displayed correctly
   */
  function testShowRawHeaders(data) {
    let requestHeaders = document.getElementById("raw-request-headers-textarea").value;
    for (let header of data.requestHeaders.headers) {
      ok(requestHeaders.includes(header.name + ": " + header.value),
        "textarea contains request headers");
    }
    let responseHeaders = document.getElementById("raw-response-headers-textarea").value;
    for (let header of data.responseHeaders.headers) {
      ok(responseHeaders.includes(header.name + ": " + header.value),
        "textarea contains response headers");
    }
  }

  /*
   * Tests that raw headers textareas are hidden and empty
   */
  function testHideRawHeaders() {
    let rawHeadersHidden = document.getElementById("raw-headers").getAttribute("hidden");
    let requestTextarea = document.getElementById("raw-request-headers-textarea");
    let responseTextarea = document.getElementById("raw-response-headers-textarea");
    ok(rawHeadersHidden, "raw headers textareas are hidden");
    ok(requestTextarea.value == "", "raw request headers textarea is empty");
    ok(responseTextarea.value == "", "raw response headers textarea is empty");
  }
});
