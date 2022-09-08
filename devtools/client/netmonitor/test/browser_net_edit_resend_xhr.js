/* Any copyright is dedicated to the Public Domain.
 *  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if editing and resending a XHR request works and the
 * cloned request retains the same cause type.
 */

add_task(async function() {
  if (
    Services.prefs.getBoolPref(
      "devtools.netmonitor.features.newEditAndResend",
      true
    )
  ) {
    ok(
      true,
      "Skip this test when pref is true, because this panel won't be default when that is the case."
    );
    return;
  }

  const { tab, monitor } = await initNetMonitor(POST_RAW_URL, {
    requestCount: 1,
  });

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  // Executes 1 XHR request
  await performRequests(monitor, tab, 1);

  // Selects 1st XHR request
  const xhrRequest = document.querySelectorAll(".request-list-item")[0];
  EventUtils.sendMouseEvent({ type: "mousedown" }, xhrRequest);

  // Stores original request for comparison of values later
  const { getSelectedRequest } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );
  const original = getSelectedRequest(store.getState());

  // Context Menu > "Edit & Resend"
  EventUtils.sendMouseEvent({ type: "contextmenu" }, xhrRequest);
  await selectContextMenuItem(monitor, "request-list-context-edit-resend");

  // 1) Wait for "Edit & Resend" panel to appear
  // 2) Click the "Send" button
  // 3) Wait till the new request appears in the list
  await waitUntil(() => document.querySelector(".custom-request-panel"));
  document.querySelector("#custom-request-send-button").click();
  await waitForNetworkEvents(monitor, 1);

  // Selects cloned request
  const clonedRequest = document.querySelectorAll(".request-list-item")[1];
  EventUtils.sendMouseEvent({ type: "mousedown" }, clonedRequest);
  const cloned = getSelectedRequest(store.getState());

  // Compares if the requests have the same cause type (XHR)
  ok(
    original.cause.type === cloned.cause.type,
    "Both requests retain the same cause type"
  );

  await teardown(monitor);
});

/**
 * Tests if editing and resending a XHR request works and the
 * new request retains the same cause type.
 */

add_task(async function() {
  if (
    Services.prefs.getBoolPref(
      "devtools.netmonitor.features.newEditAndResend",
      true
    )
  ) {
    const { tab, monitor } = await initNetMonitor(POST_RAW_URL, {
      requestCount: 1,
    });

    const { document, store, windowRequire } = monitor.panelWin;
    const Actions = windowRequire(
      "devtools/client/netmonitor/src/actions/index"
    );
    store.dispatch(Actions.batchEnable(false));

    // Executes 1 XHR request
    await performRequests(monitor, tab, 1);

    // Selects 1st XHR request
    const xhrRequest = document.querySelectorAll(".request-list-item")[0];
    EventUtils.sendMouseEvent({ type: "mousedown" }, xhrRequest);

    // Stores original request for comparison of values later
    const { getSelectedRequest } = windowRequire(
      "devtools/client/netmonitor/src/selectors/index"
    );
    const original = getSelectedRequest(store.getState());

    // Context Menu > "Edit & Resend"
    EventUtils.sendMouseEvent({ type: "contextmenu" }, xhrRequest);
    await selectContextMenuItem(monitor, "request-list-context-edit-resend");

    // 1) Wait for "Edit & Resend" panel to appear
    // 2) Wait for the Send button to be  enabled (i.e all the data is loaded)
    // 2) Click the "Send" button
    // 3) Wait till the new request appears in the list
    await waitUntil(
      () =>
        document.querySelector(".http-custom-request-panel") &&
        document.querySelector("#http-custom-request-send-button").disabled ===
          false
    );
    document.querySelector("#http-custom-request-send-button").click();
    await waitForNetworkEvents(monitor, 1);

    // Selects new request
    const newRequest = document.querySelectorAll(".request-list-item")[1];
    EventUtils.sendMouseEvent({ type: "mousedown" }, newRequest);
    const request = getSelectedRequest(store.getState());

    ok(
      original.cause.type === request.cause.type,
      "Both requests retain the same cause type"
    );

    await teardown(monitor);
  }
});
