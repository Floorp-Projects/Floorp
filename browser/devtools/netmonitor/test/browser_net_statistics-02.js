/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the network inspector view is shown when the target navigates
 * away while in the statistics view.
 */

function test() {
  initNetMonitor(STATISTICS_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let panel = aMonitor.panelWin;
    let { document, EVENTS, NetMonitorView } = panel;

    is(NetMonitorView.currentFrontendMode, "network-inspector-view",
      "The initial frontend mode is correct.");

    promise.all([
      waitFor(panel, EVENTS.PRIMED_CACHE_CHART_DISPLAYED),
      waitFor(panel, EVENTS.EMPTY_CACHE_CHART_DISPLAYED)
    ]).then(() => {
      is(NetMonitorView.currentFrontendMode, "network-statistics-view",
        "The frontend mode is currently in the statistics view.");

      waitFor(panel, EVENTS.TARGET_WILL_NAVIGATE).then(() => {
        is(NetMonitorView.currentFrontendMode, "network-inspector-view",
          "The frontend mode switched back to the inspector view.");

        waitFor(panel, EVENTS.TARGET_DID_NAVIGATE).then(() => {
          is(NetMonitorView.currentFrontendMode, "network-inspector-view",
            "The frontend mode is still in the inspector view.");

          teardown(aMonitor).then(finish);
        });
      });

      aDebuggee.location.reload();
    });

    NetMonitorView.toggleFrontendMode();
  });
}
