/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const ALL_CHANNELS = Ci.nsITelemetry.DATASET_ALL_CHANNELS;

/**
 * Test the filters_changed telemetry event.
 */
add_task(async function() {
  const { monitor } = await initNetMonitor(HTTPS_SIMPLE_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const { getDisplayedRequests } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );

  store.dispatch(Actions.batchEnable(false));

  await waitForAllNetworkUpdateEvents();
  // Remove all telemetry events (you can check about:telemetry).
  Services.telemetry.clearEvents();

  // Ensure no events have been logged
  const snapshot = Services.telemetry.snapshotEvents(ALL_CHANNELS, true);
  ok(!snapshot.parent, "No events have been logged for the main process");

  // Reload to have one request in the list.
  const wait = waitForNetworkEvents(monitor, 1);
  await waitForAllNetworkUpdateEvents();
  await navigateTo(HTTPS_SIMPLE_URL);
  await wait;

  info("Click on the 'HTML' filter");
  EventUtils.sendMouseEvent(
    { type: "click" },
    document.querySelector(".requests-list-filter-html-button")
  );

  checkTelemetryEvent(
    {
      trigger: "html",
      active: "html",
      inactive: "all,css,js,xhr,fonts,images,media,ws,other",
    },
    {
      method: "filters_changed",
    }
  );

  info("Click on the 'CSS' filter");
  EventUtils.sendMouseEvent(
    { type: "click" },
    document.querySelector(".requests-list-filter-css-button")
  );

  checkTelemetryEvent(
    {
      trigger: "css",
      active: "html,css",
      inactive: "all,js,xhr,fonts,images,media,ws,other",
    },
    {
      method: "filters_changed",
    }
  );

  info("Filter the output using the text filter input");
  setFreetextFilter(monitor, "nomatch");

  // Wait till the text filter is applied.
  await waitUntil(() => !getDisplayedRequests(store.getState()).length);

  checkTelemetryEvent(
    {
      trigger: "text",
      active: "html,css",
      inactive: "all,js,xhr,fonts,images,media,ws,other",
    },
    {
      method: "filters_changed",
    }
  );

  return teardown(monitor);
});

function setFreetextFilter(monitor, value) {
  const { document } = monitor.panelWin;

  const filterBox = document.querySelector(".devtools-filterinput");
  filterBox.focus();
  filterBox.value = "";

  for (const ch of value) {
    EventUtils.synthesizeKey(ch, {}, monitor.panelWin);
  }
}
