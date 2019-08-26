/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the select_ws_frame telemetry event.
 */
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

add_task(async function() {
  await pushPref("devtools.netmonitor.features.webSockets", true);
  const { tab, monitor } = await initNetMonitor(WS_PAGE_URL);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  // Clear all events.
  Services.telemetry.clearEvents();

  // Ensure no events have been logged.
  TelemetryTestUtils.assertNumberOfEvents(0);

  // Wait for WS connection to be established + send messages.
  await ContentTask.spawn(tab.linkedBrowser, {}, async () => {
    await content.wrappedJSObject.openConnection(1);
  });

  const requests = document.querySelectorAll(".request-list-item");
  is(requests.length, 1, "There should be one request");

  // Select the request to open the side panel.
  EventUtils.sendMouseEvent({ type: "mousedown" }, requests[0]);

  // Wait for all sent/received messages to be displayed in DevTools.
  wait = waitForDOM(
    document,
    "#messages-panel .ws-frames-list-table .ws-frame-list-item",
    2
  );

  // Click on the "Messages" panel.
  EventUtils.sendMouseEvent(
    { type: "click" },
    document.querySelector("#messages-tab")
  );
  await wait;

  // Get all messages present in the "Messages" panel.
  const frames = document.querySelectorAll(
    "#messages-panel .ws-frames-list-table .ws-frame-list-item"
  );

  // Check expected results.
  is(frames.length, 2, "There should be two frames");

  // Wait for tick, so the `mousedown` event works.
  await waitForTick();

  // Wait for the payload to be resolved (LongString)
  const payloadResolved = waitFor(
    monitor.panelWin.api,
    EVENTS.LONGSTRING_RESOLVED
  );

  // Select frame
  EventUtils.sendMouseEvent({ type: "mousedown" }, frames[0]);
  await payloadResolved;

  // Verify existence of the telemetry event.
  checkTelemetryEvent(
    {},
    {
      method: "select_ws_frame",
    }
  );

  return teardown(monitor);
});
