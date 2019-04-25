/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the log persistence telemetry event
 */
const { TelemetryTestUtils } = ChromeUtils.import("resource://testing-common/TelemetryTestUtils.jsm");

add_task(async function() {
  const { monitor } = await initNetMonitor(SINGLE_GET_URL);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  // Clear all events
  Services.telemetry.clearEvents();

  // Ensure no events have been logged
  TelemetryTestUtils.assertNumberOfEvents(0);

  // Get log persistence toggle button
  const logPersistToggle = document.getElementById("devtools-persistlog-checkbox");

  // Click on the toggle - "true" and make sure it was set to correct value
  logPersistToggle.click();
  await waitUntil(() => logPersistToggle.checked === true);

  // Click a second time - "false" and make sure it was set to correct value
  logPersistToggle.click();
  await waitUntil(() => logPersistToggle.checked === false);

  const expectedEvents = [
    {
      category: "devtools.main",
      method: "persist_changed",
      object: "netmonitor",
      value: "true",
    },
    {
      category: "devtools.main",
      method: "persist_changed",
      object: "netmonitor",
      value: "false",
    },
  ];

  const filter = {
    category: "devtools.main",
    method: "persist_changed",
    object: "netmonitor",
  };

  // Will compare filtered events to event list above
  await TelemetryTestUtils.assertEvents(expectedEvents, filter);

  // Set Persist log preference back to false
  Services.prefs.setBoolPref("devtools.netmonitor.persistlog", false);

  return teardown(monitor);
});
