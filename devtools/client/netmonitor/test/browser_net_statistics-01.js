/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the statistics view is populated correctly.
 */

add_task(function* () {
  let { monitor } = yield initNetMonitor(STATISTICS_URL);
  info("Starting test... ");

  let panel = monitor.panelWin;
  let { $, $all, EVENTS, NetMonitorView, gStore, windowRequire } = panel;
  let Actions = windowRequire("devtools/client/netmonitor/actions/index");

  is(NetMonitorView.currentFrontendMode, "network-inspector-view",
   "The initial frontend mode is correct.");

  is($("#primed-cache-chart").childNodes.length, 0,
    "There should be no primed cache chart created yet.");
  is($("#empty-cache-chart").childNodes.length, 0,
    "There should be no empty cache chart created yet.");

  let onChartDisplayed = Promise.all([
    panel.once(EVENTS.PRIMED_CACHE_CHART_DISPLAYED),
    panel.once(EVENTS.EMPTY_CACHE_CHART_DISPLAYED)
  ]);
  let onPlaceholderDisplayed = panel.once(EVENTS.PLACEHOLDER_CHARTS_DISPLAYED);

  info("Displaying statistics view");
  gStore.dispatch(Actions.openStatistics(true));
  is(NetMonitorView.currentFrontendMode, "network-statistics-view",
   "The current frontend mode is correct.");

  info("Waiting for placeholder to display");
  yield onPlaceholderDisplayed;
  is($("#primed-cache-chart").childNodes.length, 1,
    "There should be a placeholder primed cache chart created now.");
  is($("#empty-cache-chart").childNodes.length, 1,
    "There should be a placeholder empty cache chart created now.");

  is($all(".pie-chart-container[placeholder=true]").length, 2,
    "Two placeholder pie chart appear to be rendered correctly.");
  is($all(".table-chart-container[placeholder=true]").length, 2,
    "Two placeholder table chart appear to be rendered correctly.");

  info("Waiting for chart to display");
  yield onChartDisplayed;
  is($("#primed-cache-chart").childNodes.length, 1,
    "There should be a real primed cache chart created now.");
  is($("#empty-cache-chart").childNodes.length, 1,
    "There should be a real empty cache chart created now.");

  yield waitUntil(
    () => $all(".pie-chart-container:not([placeholder=true])").length == 2);
  ok(true, "Two real pie charts appear to be rendered correctly.");

  yield waitUntil(
    () => $all(".table-chart-container:not([placeholder=true])").length == 2);
  ok(true, "Two real table charts appear to be rendered correctly.");

  yield teardown(monitor);
});
