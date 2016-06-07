/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the network inspector view is shown when the target navigates
 * away while in the statistics view.
 */

add_task(function* () {
  let [, debuggee, monitor] = yield initNetMonitor(STATISTICS_URL);
  info("Starting test... ");

  let panel = monitor.panelWin;
  let { EVENTS, NetMonitorView } = panel;
  is(NetMonitorView.currentFrontendMode, "network-inspector-view",
      "The initial frontend mode is correct.");

  let onChartDisplayed = Promise.all([
    waitFor(panel, EVENTS.PRIMED_CACHE_CHART_DISPLAYED),
    waitFor(panel, EVENTS.EMPTY_CACHE_CHART_DISPLAYED)
  ]);

  info("Displaying statistics view");
  NetMonitorView.toggleFrontendMode();
  yield onChartDisplayed;
  is(NetMonitorView.currentFrontendMode, "network-statistics-view",
        "The frontend mode is currently in the statistics view.");

  info("Reloading page");
  let onWillNavigate = waitFor(panel, EVENTS.TARGET_WILL_NAVIGATE);
  let onDidNavigate = waitFor(panel, EVENTS.TARGET_DID_NAVIGATE);
  debuggee.location.reload();
  yield onWillNavigate;
  is(NetMonitorView.currentFrontendMode, "network-inspector-view",
          "The frontend mode switched back to the inspector view.");
  yield onDidNavigate;
  is(NetMonitorView.currentFrontendMode, "network-inspector-view",
            "The frontend mode is still in the inspector view.");
  yield teardown(monitor);
});
