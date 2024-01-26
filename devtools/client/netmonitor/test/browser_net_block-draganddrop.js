/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test blocking and unblocking a request.
 */

add_task(async function () {
  class DataTransfer {
    constructor() {
      this.BLOCKING_URL =
        "https://example.com/browser/devtools/client/netmonitor/test/html_simple-test-page.html";
      this.getDataTrigger = false;
      this.setDataTrigger = false;
      this.data = "";
    }

    setData(format, data) {
      this.setDataTrigger = true;
      Assert.strictEqual(
        format,
        "text/plain",
        'setData passed valid "text/plain" format'
      );
      Assert.strictEqual(
        data,
        this.BLOCKING_URL,
        "Data matches the expected URL"
      );
      this.data = data;
    }

    getData(format) {
      this.getDataTrigger = true;
      Assert.strictEqual(
        format,
        "text/plain",
        'getData passed valid "text/plain" format'
      );
      return this.data;
    }

    wasGetDataTriggered() {
      return this.getDataTrigger;
    }

    wasSetDataTriggered() {
      return this.setDataTrigger;
    }
  }

  const dataTransfer = new DataTransfer();

  const { tab, monitor } = await initNetMonitor(HTTPS_SIMPLE_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const { getSelectedRequest } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  // Open the request blocking panel
  store.dispatch(Actions.toggleRequestBlockingPanel());

  // Reload to have one request in the list
  let waitForEvents = waitForNetworkEvents(monitor, 1);
  await navigateTo(HTTPS_SIMPLE_URL);
  await waitForEvents;

  // Capture normal request
  let normalRequestState;
  let normalRequestSize;
  {
    const requestBlockingPanel = document.querySelector(
      ".request-blocking-panel"
    );
    const firstRequest = document.querySelectorAll(".request-list-item")[0];
    const waitForHeaders = waitUntil(() =>
      document.querySelector(".headers-overview")
    );
    EventUtils.sendMouseEvent({ type: "mousedown" }, firstRequest);
    await waitForHeaders;
    normalRequestState = getSelectedRequest(store.getState());
    normalRequestSize = firstRequest.querySelector(
      ".requests-list-transferred"
    ).textContent;
    info("Captured normal request");

    // Drag and drop the list item
    const createBubbledEvent = (type, props = {}) => {
      const event = new Event(type, { bubbles: true });
      Object.assign(event, props);
      return event;
    };

    info('Dispatching "dragstart" event on first item of request list');
    firstRequest.dispatchEvent(
      createBubbledEvent("dragstart", {
        clientX: 0,
        clientY: 0,
        dataTransfer,
      })
    );

    Assert.strictEqual(
      dataTransfer.wasSetDataTriggered(),
      true,
      'setData() was called during the "dragstart" event'
    );

    info('Dispatching "drop" event on request blocking list');
    requestBlockingPanel.dispatchEvent(
      createBubbledEvent("drop", {
        clientX: 0,
        clientY: 1,
        dataTransfer,
      })
    );

    Assert.strictEqual(
      dataTransfer.wasGetDataTriggered(),
      true,
      'getData() was called during the "drop" event'
    );

    const onRequestBlocked = waitForDispatch(
      store,
      "REQUEST_BLOCKING_UPDATE_COMPLETE"
    );

    info("Wait for dropped request to be blocked");
    await onRequestBlocked;
    info("Dropped request is now blocked");
  }

  // Reload to have one request in the list
  info("Reloading to check block");
  // We can't use the normal waiting methods because a canceled request won't send all
  // the extra update packets.
  waitForEvents = waitForNetworkEvents(monitor, 1);
  tab.linkedBrowser.reload();
  await waitForEvents;

  // Capture blocked request, then unblock
  let blockedRequestState;
  let blockedRequestSize;
  {
    const firstRequest = document.querySelectorAll(".request-list-item")[0];
    blockedRequestSize = firstRequest.querySelector(
      ".requests-list-transferred"
    ).textContent;
    EventUtils.sendMouseEvent({ type: "mousedown" }, firstRequest);
    blockedRequestState = getSelectedRequest(store.getState());
    info("Captured blocked request");
  }

  ok(!normalRequestState.blockedReason, "Normal request is not blocked");
  ok(!normalRequestSize.includes("Blocked"), "Normal request has a size");

  ok(blockedRequestState.blockedReason, "Blocked request is blocked");
  ok(
    blockedRequestSize.includes("Blocked"),
    "Blocked request shows reason as size"
  );

  return teardown(monitor);
});
