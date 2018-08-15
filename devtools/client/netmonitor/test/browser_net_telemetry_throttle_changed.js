/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const OPTOUT = Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTOUT;

/**
 * Test the throttle_change telemetry event.
 */
add_task(async function() {
  const { monitor } = await initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  // Remove all telemetry events.
  Services.telemetry.clearEvents();

  // Ensure no events have been logged
  const snapshot = Services.telemetry.snapshotEvents(OPTOUT, true);
  ok(!snapshot.parent, "No events have been logged for the main process");

  document.querySelector("#global-network-throttling-selector").click();
  monitor.panelWin.parent.document.querySelector("menuitem[label='GPRS']").click();
  await waitFor(monitor.panelWin.api, EVENTS.THROTTLING_CHANGED);

  // Verify existence of the telemetry event.
  checkTelemetryEvent({
    mode: "GPRS",
  }, {
    method: "throttle_changed",
  });

  return teardown(monitor);
});
