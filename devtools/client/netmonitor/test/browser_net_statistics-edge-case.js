/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the statistics panel displays correctly for a page containing
 * requests which have been known to create issues in the past for this Panel:
 * - cached image (http-on-image-cache-response) requests in the content process
 * - long polling requests remaining open for a long time
 */

add_task(async function () {
  // We start the netmonitor on a basic page to avoid opening the panel on
  // an incomplete polling request.
  const { monitor } = await initNetMonitor(SIMPLE_URL, {
    enableCache: true,
  });

  // The navigation should lead to 3 requests (html + image + long polling).
  // Additionally we will have a 4th request to call unblock(), so there are 4
  // requests in total.
  // However we need to make sure that unblock() is only called once the long
  // polling request has been started, so we wait for network events in 2 sets:
  // - first the 3 initial requests, with only 2 completing
  // - then the unblock request, bundled with the completion of the long polling
  let onNetworkEvents = waitForNetworkEvents(monitor, 3, {
    expectedPayloadReady: 2,
    expectedEventTimings: 2,
  });

  // Here we explicitly do not await on navigateTo.
  // The netmonitor will not emit "reloaded" if there are pending requests,
  // so if the long polling request already started, we will never receive the
  // event. Waiting for the network events should be sufficient here.
  const onNavigationCompleted = navigateTo(STATISTICS_EDGE_CASE_URL);

  info("Wait for the 3 first network events (initial)");
  await onNetworkEvents;

  // Prepare to wait for the second set of network events.
  onNetworkEvents = waitForNetworkEvents(monitor, 1, {
    expectedPayloadReady: 2,
    expectedEventTimings: 2,
  });

  // Calling unblock() should allow for the polling request to be displayed and
  // to complete, so we can consistently expect 2 events and 2 timings.
  info("Call unblock()");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () =>
    content.wrappedJSObject.unblock()
  );

  info("Wait for polling and unblock (initial)");
  await onNetworkEvents;

  info("Wait for the navigation to complete");
  await onNavigationCompleted;

  // Opening the statistics panel will trigger a reload, expect the same requests
  // again, we use the same pattern to wait for network events.
  onNetworkEvents = waitForNetworkEvents(monitor, 3, {
    expectedPayloadReady: 2,
    expectedEventTimings: 2,
  });

  info("Open the statistics panel");
  const panel = monitor.panelWin;
  const { document, store, windowRequire, connector } = panel;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.openStatistics(connector, true));

  await waitFor(
    () => !!document.querySelector(".statistics-panel"),
    "The statistics panel is displayed"
  );

  await waitFor(
    () =>
      document.querySelectorAll(
        ".table-chart-container:not([placeholder=true])"
      ).length == 2,
    "Two real table charts appear to be rendered correctly."
  );

  info("Close statistics panel");
  store.dispatch(Actions.openStatistics(connector, false));

  await waitFor(
    () => !!document.querySelector(".monitor-panel"),
    "The regular netmonitor panel is displayed"
  );
  info("Wait for the 3 first network events (after opening statistics panel)");
  await onNetworkEvents;

  onNetworkEvents = waitForNetworkEvents(monitor, 1, {
    expectedPayloadReady: 2,
    expectedEventTimings: 2,
  });

  info("Call unblock()");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () =>
    content.wrappedJSObject.unblock()
  );

  // We need to cleanly wait for all events to be finished, otherwise the UI
  // will throw many unhandled promise rejections.
  info("Wait for polling and unblock (after opening statistics panel)");
  await onNetworkEvents;

  await teardown(monitor);
});
