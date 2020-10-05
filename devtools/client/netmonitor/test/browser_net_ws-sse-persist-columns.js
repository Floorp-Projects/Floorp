/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test columns' state for WS and SSE connection.
 */

function shallowEqual(obj1, obj2) {
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

add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["devtools.netmonitor.features.serverSentEvents", true],
      ["devtools.netmonitor.features.webSockets", true],
    ],
  });

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

  // Select the SSE request.
  EventUtils.sendMouseEvent({ type: "mousedown" }, requests[1]);

  store.dispatch(Actions.toggleMessageColumn("lastEventId"));
  store.dispatch(Actions.toggleMessageColumn("eventName"));
  store.dispatch(Actions.toggleMessageColumn("retry"));

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
