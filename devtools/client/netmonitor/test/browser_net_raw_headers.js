/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if showing raw headers works.
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(POST_DATA_URL);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const {
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

  // Execute requests.
  await performRequests(monitor, tab, 2);

  wait = waitForDOM(document, "#headers-panel .tree-section", 2);
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]);
  await wait;

  wait = waitForDOM(document, ".raw-headers-container textarea", 2);
  EventUtils.sendMouseEvent({ type: "click" }, getRawHeadersButton());
  await wait;

  testRawHeaderButtonStyle(true);

  testShowRawHeaders(getSortedRequests(store.getState()).get(0));

  EventUtils.sendMouseEvent({ type: "click" }, getRawHeadersButton());

  testRawHeaderButtonStyle(false);

  testHideRawHeaders(document);

  return teardown(monitor);

  /**
   * Tests that checked, aria-pressed style is applied correctly
   *
   * @param checked
   *        flag indicating whether button is pressed or not
   */
  function testRawHeaderButtonStyle(checked) {
    const rawHeadersButton = getRawHeadersButton();

    if (checked) {
      is(rawHeadersButton.classList.contains("checked"), true,
        "The 'Raw Headers' button should have a 'checked' class.");
      is(rawHeadersButton.getAttribute("aria-pressed"), "true",
        "The 'Raw Headers' button should have the 'aria-pressed' attribute set to true");
    } else {
      is(rawHeadersButton.classList.contains("checked"), false,
        "The 'Raw Headers' button should not have a 'checked' class.");
      is(rawHeadersButton.getAttribute("aria-pressed"), "false",
        "The 'Raw Headers' button should have the 'aria-pressed' attribute set to false");
    }
  }

  /*
   * Tests that raw headers were displayed correctly
   */
  function testShowRawHeaders(data) {
    const requestHeaders = document
      .querySelectorAll(".raw-headers-container textarea")[0].value;
    for (const header of data.requestHeaders.headers) {
      ok(requestHeaders.includes(header.name + ": " + header.value),
        "textarea contains request headers");
    }
    const responseHeaders = document
      .querySelectorAll(".raw-headers-container textarea")[1].value;
    for (const header of data.responseHeaders.headers) {
      ok(responseHeaders.includes(header.name + ": " + header.value),
        "textarea contains response headers");
    }
  }

  /*
   * Tests that raw headers textareas are hidden
   */
  function testHideRawHeaders() {
    ok(!document.querySelector(".raw-headers-container"),
      "raw request headers textarea is empty");
  }

  /**
   * Returns the 'Raw Headers' button
   */
  function getRawHeadersButton() {
    return document.querySelectorAll(".headers-summary .devtools-button")[2];
  }
});
