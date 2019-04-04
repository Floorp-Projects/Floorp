/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const ALL_CHANNELS = Ci.nsITelemetry.DATASET_ALL_CHANNELS;

/**
 * Test the sidepanel_changed telemetry event.
 */
add_task(async function() {
  const { tab, monitor } = await initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  // Remove all telemetry events (you can check about:telemetry).
  Services.telemetry.clearEvents();

  // Ensure no events have been logged
  const snapshot = Services.telemetry.snapshotEvents(ALL_CHANNELS, true);
  ok(!snapshot.parent, "No events have been logged for the main process");

  // Reload to have one request in the list.
  const waitForEvents = waitForNetworkEvents(monitor, 1);
  BrowserTestUtils.loadURI(tab.linkedBrowser, SIMPLE_URL);
  await waitForEvents;

  // Click on a request and wait till the default "Headers" side panel is opened.
  info("Click on a request");
  const waitForHeaders = waitUntil(() => document.querySelector(".headers-overview"));
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]);
  await waitForHeaders;
  await waitForRequestData(store, ["requestHeaders", "responseHeaders"]);

  // Click on the Cookies panel and wait till it's opened.
  info("Click on the Cookies panel");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#cookies-tab"));
  await waitForRequestData(store, ["requestCookies", "responseCookies"]);

  checkTelemetryEvent({
    oldpanel: "headers",
    newpanel: "cookies",
  }, {
    method: "sidepanel_changed",
  });

  return teardown(monitor);
});
