/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that WS connection created while the page is still loading
 * is properly tracked and there are WS frames displayed in the
 * Messages side panel.
 */
add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["devtools.netmonitor.features.webSockets", true]],
  });

  const { monitor } = await initNetMonitor(SIMPLE_URL, { requestCount: 1 });

  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  // Make the WS Messages side panel the default so, we avoid
  // request headers from the backend by selecting the Headers
  // panel
  store.dispatch(Actions.selectDetailsPanelTab("messages"));

  // Load page that opens WS connection during the load time.
  const waitForEvents = waitForNetworkEvents(monitor, 3);
  await navigateTo(WS_PAGE_EARLY_CONNECTION_URL);
  await waitForEvents;

  const requests = document.querySelectorAll(
    ".request-list-item .requests-list-file"
  );
  is(requests.length, 3, "There should be three requests");

  // Get index of the WS connection request.
  const index = Array.from(requests).findIndex(element => {
    return element.textContent === "file_ws_backend";
  });

  ok(index !== -1, "There must be one WS connection request");

  // Select the connection request to see WS frames in the side panel.
  EventUtils.sendMouseEvent({ type: "mousedown" }, requests[index]);

  info("Waiting for WS frames...");

  // Wait for two frames to be displayed in the panel
  await waitForDOMIfNeeded(
    document,
    "#messages-panel .ws-frames-list-table .ws-frame-list-item",
    2
  );

  // Check the payload of the first frame.
  const firstFramePayload = document.querySelector(
    "#messages-panel .ws-frames-list-table .ws-frame-list-item .ws-frames-list-payload"
  );
  is(firstFramePayload.textContent.trim(), "readyState:loading");

  await teardown(monitor);
});
