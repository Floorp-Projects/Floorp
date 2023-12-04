/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if showing raw headers works.
 */

add_task(async function () {
  const { tab, monitor } = await initNetMonitor(POST_DATA_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const { getSortedRequests } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );

  store.dispatch(Actions.batchEnable(false));

  info("Add block pattern to block the second request");
  await addBlockedRequest(
    "sjs_simple-test-server.sjs?foo=bar&baz=42&type=multipart",
    monitor
  );

  // Execute requests.
  await performRequests(monitor, tab, 2);

  info("Test the request headers for a normal request");

  let wait = waitForDOM(document, "#headers-panel .accordion-item", 2);
  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]
  );
  await wait;

  wait = waitForDOM(document, "#responseHeaders textarea.raw-headers", 1);
  EventUtils.sendMouseEvent({ type: "click" }, getRawHeadersToggle("RESPONSE"));
  await wait;

  wait = waitForDOM(document, "#requestHeaders textarea.raw-headers", 1);
  EventUtils.sendMouseEvent({ type: "click" }, getRawHeadersToggle("REQUEST"));
  await wait;

  testRawHeaderToggleStyle(true);
  testAllRawHeaders(getSortedRequests(store.getState())[0]);

  EventUtils.sendMouseEvent({ type: "click" }, getRawHeadersToggle("RESPONSE"));
  EventUtils.sendMouseEvent({ type: "click" }, getRawHeadersToggle("REQUEST"));

  testRawHeaderToggleStyle(false);
  testHideRawHeaders(document);

  info("Test the request headers for a blocked request");

  const waitForRequestHeaderPanel = waitForDOM(
    document,
    "#headers-panel .accordion-item",
    1
  );
  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[1]
  );
  await waitForRequestHeaderPanel;

  info("Toggle on the request headers raw toggle button");
  wait = waitForDOM(document, "#requestHeaders textarea.raw-headers", 1);
  EventUtils.sendMouseEvent(
    { type: "click" },
    document.querySelector(".devtools-checkbox-toggle")
  );
  await wait;

  testRawHeaders(
    "request",
    document.querySelector("textarea.raw-headers").value,
    getSortedRequests(store.getState())[1].requestHeaders,
    "POST /browser/devtools/client/netmonitor/test/sjs_simple-test-server.sjs?foo=bar&baz=42&type=multipart HTTP/1.1"
  );

  return teardown(monitor);

  /**
   * Tests that checked is applied correctly
   *
   * @param checked
   *        flag indicating whether toggle is checked or not
   */
  function testRawHeaderToggleStyle(checked) {
    const rawHeadersRequestToggle = getRawHeadersToggle("REQUEST");
    const rawHeadersResponseToggle = getRawHeadersToggle("RESPONSE");

    if (checked) {
      is(
        rawHeadersRequestToggle.checked,
        true,
        "The 'Raw Request Headers' toggle should be 'checked'"
      );
      is(
        rawHeadersResponseToggle.checked,
        true,
        "The 'Raw Response Headers' toggle should be 'checked'"
      );
    } else {
      is(
        rawHeadersRequestToggle.checked,
        false,
        "The 'Raw Request Headers' toggle should NOT be 'checked'"
      );
      is(
        rawHeadersResponseToggle.checked,
        false,
        "The 'Raw Response Headers' toggle should NOT be 'checked'"
      );
    }
  }

  /*
   * Tests that raw headers were displayed correctly
   */

  async function testAllRawHeaders(data) {
    // Request headers are rendered first, so it is element with index 1
    testRawHeaders(
      "request",
      document.querySelectorAll("textarea.raw-headers")[1].value,
      data.requestHeaders,
      "POST /browser/devtools/client/netmonitor/test/sjs_simple-test-server.sjs?foo=bar&baz=42&valueWithEqualSign=hijk=123=mnop&type=urlencoded HTTP/1.1"
    );

    // Response headers are rendered first, so it is element with index 0
    testRawHeaders(
      "response",
      document.querySelector("textarea.raw-headers").value,
      data.responseHeaders,
      "HTTP/1.1 200 Och Aye"
    );
  }

  function testRawHeaders(
    headerType,
    rawTextContent,
    headersData,
    expectedPreText
  ) {
    info(
      `Assert that the ${headerType} raw headers first line version is rendered correctly`
    );
    ok(
      rawTextContent.startsWith(expectedPreText),
      `The first line of the raw ${headerType} header content is correct`
    );

    info(`Assert the ${headerType} headers and values`);
    for (const header of headersData.headers) {
      ok(
        rawTextContent.includes(header.name + ": " + header.value),
        `textarea contains ${header.name} ${headerType} header`
      );
    }
  }

  /*
   * Tests that raw headers textareas are hidden
   */
  function testHideRawHeaders() {
    ok(
      !document.querySelector(".raw-headers-container"),
      "raw request headers textarea is empty"
    );
  }

  /**
   * Returns the 'Raw Headers' button
   */
  function getRawHeadersToggle(rawHeaderType) {
    if (rawHeaderType === "RESPONSE") {
      // Response header is first displayed
      return document.querySelectorAll(".devtools-checkbox-toggle")[0];
    }
    return document.querySelectorAll(".devtools-checkbox-toggle")[1];
  }
});
