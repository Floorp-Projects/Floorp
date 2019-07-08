/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if "Referrer Policy" is displayed in the header panel.
 */
add_task(async function() {
  const { tab, monitor } = await initNetMonitor(POST_RAW_URL);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  // Execute request.
  await performRequests(monitor, tab, 1);

  // Wait until the tab panel summary is displayed
  wait = waitUntil(
    () => document.querySelectorAll(".tabpanel-summary-label")[0]
  );
  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]
  );
  await wait;

  const referrerPolicyIndex = 5;
  const referrerPolicyHeader = document.querySelectorAll(
    ".tabpanel-summary-label"
  )[referrerPolicyIndex];
  const referrerPolicyValue = document.querySelectorAll(
    ".tabpanel-summary-value"
  )[referrerPolicyIndex];

  is(
    referrerPolicyHeader.textContent === "Referrer Policy:",
    true,
    '"Referrer Policy" header is displayed in the header panel.'
  );

  is(
    referrerPolicyValue.textContent === "no-referrer-when-downgrade",
    true,
    "The referrer policy value is reflected correctly."
  );

  return teardown(monitor);
});
