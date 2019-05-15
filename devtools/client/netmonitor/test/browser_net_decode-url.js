/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if "Request URL" containing "#" in its query is correctly decoded.
 */
add_task(async function() {
  const { tab, monitor } = await initNetMonitor(POST_RAW_URL_WITH_HASH);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  // Execute request.
  await performRequests(monitor, tab, 1);

  // Wait until the tab panel summary is displayed
  wait = waitUntil(() =>
    document.querySelectorAll(".tabpanel-summary-label")[0]);
  EventUtils.sendMouseEvent({ type: "mousedown" },
  document.querySelectorAll(".request-list-item")[0]);
  await wait;

  const requestURL = document.querySelectorAll(".tabpanel-summary-value")[0];

  is(requestURL.textContent.endsWith("foo+%23+bar"),
  true, "\"Request URL\" containing '#' is correctly decoded.");

  return teardown(monitor);
});
