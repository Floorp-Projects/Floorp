/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the statistics view is populated correctly.
 */

function test() {
  initNetMonitor(STATISTICS_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let panel = aMonitor.panelWin;
    let { document, $, $all, EVENTS, NetMonitorView } = panel;

    is(NetMonitorView.currentFrontendMode, "network-inspector-view",
      "The initial frontend mode is correct.");

    is($("#primed-cache-chart").childNodes.length, 0,
      "There should be no primed cache chart created yet.");
    is($("#empty-cache-chart").childNodes.length, 0,
      "There should be no empty cache chart created yet.");

    waitFor(panel, EVENTS.PLACEHOLDER_CHARTS_DISPLAYED).then(() => {
      is($("#primed-cache-chart").childNodes.length, 1,
        "There should be a placeholder primed cache chart created now.");
      is($("#empty-cache-chart").childNodes.length, 1,
        "There should be a placeholder empty cache chart created now.");

      is($all(".pie-chart-container[placeholder=true]").length, 2,
        "Two placeholder pie chart appear to be rendered correctly.");
      is($all(".table-chart-container[placeholder=true]").length, 2,
        "Two placeholder table chart appear to be rendered correctly.");

      promise.all([
        waitFor(panel, EVENTS.PRIMED_CACHE_CHART_DISPLAYED),
        waitFor(panel, EVENTS.EMPTY_CACHE_CHART_DISPLAYED)
      ]).then(() => {
        is($("#primed-cache-chart").childNodes.length, 1,
          "There should be a real primed cache chart created now.");
        is($("#empty-cache-chart").childNodes.length, 1,
          "There should be a real empty cache chart created now.");

        is($all(".pie-chart-container:not([placeholder=true])").length, 2,
          "Two real pie chart appear to be rendered correctly.");
        is($all(".table-chart-container:not([placeholder=true])").length, 2,
          "Two real table chart appear to be rendered correctly.");

        teardown(aMonitor).then(finish);
      });
    });

    NetMonitorView.toggleFrontendMode();

    is(NetMonitorView.currentFrontendMode, "network-statistics-view",
      "The current frontend mode is correct.");
  });
}
