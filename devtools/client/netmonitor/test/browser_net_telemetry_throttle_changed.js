/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const ALL_CHANNELS = Ci.nsITelemetry.DATASET_ALL_CHANNELS;

/**
 * Test the throttle_change telemetry event.
 */
add_task(async function() {
  const { monitor, toolbox } = await initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  // Remove all telemetry events.
  Services.telemetry.clearEvents();

  // Ensure no events have been logged
  const snapshot = Services.telemetry.snapshotEvents(ALL_CHANNELS, true);
  ok(!snapshot.parent, "No events have been logged for the main process");

  document.getElementById("network-throttling-menu").click();
  // Throttling menu items cannot be retrieved by id so we can't use getContextMenuItem
  // here. Instead use querySelector on the toolbox top document, where the context menu
  // will be rendered.
  toolbox.topWindow.document.querySelector("menuitem[label='GPRS']").click();
  await waitFor(monitor.panelWin.api, EVENTS.THROTTLING_CHANGED);

  // Verify existence of the telemetry event.
  checkTelemetryEvent({
    mode: "GPRS",
  }, {
    method: "throttle_changed",
  });

  return teardown(monitor);
});
