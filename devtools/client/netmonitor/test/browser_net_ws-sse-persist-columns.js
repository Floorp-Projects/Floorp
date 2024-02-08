/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test columns' state for WS and SSE connection.
 */

function shallowArrayEqual(arr1, arr2) {
  if (arr1.length !== arr2.length) {
    return false;
  }
  for (let i = 0; i < arr1.length; i++) {
    if (
      (arr2[i] instanceof RegExp && !arr2[i].test(arr1[i])) ||
      (typeof arr2[i] === "string" && arr2[i] !== arr1[i])
    ) {
      return false;
    }
  }
  return true;
}

function shallowObjectEqual(obj1, obj2) {
  const k1 = Object.keys(obj1);
  const k2 = Object.keys(obj2);

  if (k1.length !== k2.length) {
    return false;
  }

  for (const key of k1) {
    if (obj1[key] !== obj2[key]) {
      return false;
    }
  }

  return true;
}

function shallowEqual(obj1, obj2) {
  if (Array.isArray(obj1) && Array.isArray(obj2)) {
    return shallowArrayEqual(obj1, obj2);
  }
  return shallowObjectEqual(obj1, obj2);
}

add_task(async function () {
  const { tab, monitor } = await initNetMonitor(
    "http://mochi.test:8888/browser/devtools/client/netmonitor/test/html_ws-sse-test-page.html",
    {
      requestCount: 1,
    }
  );
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  const onNetworkEvents = waitForNetworkEvents(monitor, 2);
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    await content.wrappedJSObject.openWsConnection(1);
    // Running openSseConnection() here causes intermittent behavior.
  });
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    await content.wrappedJSObject.openSseConnection();
  });
  await onNetworkEvents;

  const requests = document.querySelectorAll(".request-list-item");
  is(requests.length, 2, "There should be two requests");

  // Select the WS request.
  EventUtils.sendMouseEvent({ type: "mousedown" }, requests[0]);

  store.dispatch(Actions.toggleMessageColumn("size"));
  store.dispatch(Actions.toggleMessageColumn("opCode"));
  store.dispatch(Actions.toggleMessageColumn("maskBit"));
  store.dispatch(Actions.toggleMessageColumn("finBit"));
  clickOnSidebarTab(document, "response");

  // Get all messages present in the "Response" panel
  let frames = await waitFor(() => {
    const nodeList = document.querySelectorAll(
      "#messages-view .message-list-table .message-list-item"
    );
    return nodeList.length === 2 ? nodeList : null;
  });

  // Check expected results

  is(frames.length, 2, "There should be two frames");

  let columnHeaders = Array.prototype.map.call(
    document.querySelectorAll(
      "#messages-view .message-list-headers .button-text"
    ),
    node => node.textContent
  );

  is(
    shallowEqual(columnHeaders, [
      "Data",
      "Size",
      "OpCode",
      "MaskBit",
      "FinBit",
      "Time",
    ]),
    true,
    "WS Column headers are in correct order"
  );

  // Get column values of first row for WS.
  let columnValues = Array.prototype.map.call(frames, frame =>
    Array.prototype.map.call(
      frame.querySelectorAll(".message-list-column"),
      column => column.textContent.trim()
    )
  )[0];

  is(
    shallowEqual(columnValues, [
      "Payload 0",
      "9 B",
      "1",
      "true",
      "true",
      // Time format is "hh:mm:ss.mmm".
      /\d+:\d+:\d+\.\d+/,
    ]),
    true,
    "WS Column values are in correct order"
  );

  // Select the SSE request.
  EventUtils.sendMouseEvent({ type: "mousedown" }, requests[1]);

  store.dispatch(Actions.toggleMessageColumn("lastEventId"));
  store.dispatch(Actions.toggleMessageColumn("eventName"));
  store.dispatch(Actions.toggleMessageColumn("retry"));

  await waitFor(
    () =>
      document.querySelectorAll(
        "#messages-view .message-list-headers .button-text"
      ).length === 5
  );
  frames = document.querySelectorAll(
    "#messages-view .message-list-table .message-list-item"
  );

  columnHeaders = Array.prototype.map.call(
    document.querySelectorAll(
      "#messages-view .message-list-headers .button-text"
    ),
    node => node.textContent
  );

  is(
    shallowEqual(columnHeaders, [
      "Data",
      "Time",
      "Event Name",
      "Last Event ID",
      "Retry",
    ]),
    true,
    "SSE Column headers are in correct order"
  );

  // Get column values of first row for SSE.
  columnValues = Array.prototype.map.call(frames, frame =>
    Array.prototype.map.call(
      frame.querySelectorAll(".message-list-column"),
      column => column.textContent.trim()
    )
  )[0];

  is(
    shallowEqual(columnValues, [
      "Why so serious?",
      /\d+:\d+:\d+\.\d+/,
      "message",
      "",
      "5000",
    ]),
    true,
    "SSE Column values are in correct order"
  );

  // Select the WS request again.
  EventUtils.sendMouseEvent({ type: "mousedown" }, requests[0]);
  is(
    shallowEqual(store.getState().messages.columns, {
      data: true,
      time: true,
      size: true,
      opCode: true,
      maskBit: true,
      finBit: true,
    }),
    true,
    "WS columns should persist after request switch"
  );

  // Select the SSE request again.
  EventUtils.sendMouseEvent({ type: "mousedown" }, requests[1]);
  is(
    shallowEqual(store.getState().messages.columns, {
      data: true,
      time: true,
      size: false,
      lastEventId: true,
      eventName: true,
      retry: true,
    }),
    true,
    "SSE columns should persist after request switch"
  );

  // Reset SSE columns.
  store.dispatch(Actions.resetMessageColumns());

  // Switch to WS request again.
  EventUtils.sendMouseEvent({ type: "mousedown" }, requests[0]);
  is(
    shallowEqual(store.getState().messages.columns, {
      data: true,
      time: true,
      size: true,
      opCode: true,
      maskBit: true,
      finBit: true,
    }),
    true,
    "WS columns should not reset after resetting SSE columns"
  );

  // Reset WS columns.
  store.dispatch(Actions.resetMessageColumns());

  // Switch to SSE request again.
  EventUtils.sendMouseEvent({ type: "mousedown" }, requests[1]);
  is(
    shallowEqual(store.getState().messages.columns, {
      data: true,
      time: true,
      size: false,
      lastEventId: false,
      eventName: false,
      retry: false,
    }),
    true,
    "SSE columns' reset state should persist after request switch"
  );

  // Switch to WS request again.
  EventUtils.sendMouseEvent({ type: "mousedown" }, requests[0]);
  is(
    shallowEqual(store.getState().messages.columns, {
      data: true,
      time: true,
      size: false,
      opCode: false,
      maskBit: false,
      finBit: false,
    }),
    true,
    "WS columns' reset state should persist after request switch"
  );

  // Close WS connection.
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    await content.wrappedJSObject.closeWsConnection();
  });

  return teardown(monitor);
});
