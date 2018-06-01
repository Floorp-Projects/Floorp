/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the statistics panel displays correctly.
 */

add_task(async function() {
  const { monitor } = await initNetMonitor(STATISTICS_URL);
  info("Starting test... ");

  const panel = monitor.panelWin;
  const { document, store, windowRequire, connector } = panel;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  ok(document.querySelector(".monitor-panel"),
    "The current main panel is correct.");

  info("Displaying statistics panel");
  store.dispatch(Actions.openStatistics(connector, true));

  ok(document.querySelector(".statistics-panel"),
    "The current main panel is correct.");

  info("Waiting for placeholder to display");

  await waitUntil(
    () => document.querySelectorAll(".pie-chart-container[placeholder=true]")
                  .length == 2);
  ok(true, "Two placeholder pie charts appear to be rendered correctly.");

  await waitUntil(
    () => document.querySelectorAll(".table-chart-container[placeholder=true]")
                  .length == 2);
  ok(true, "Two placeholde table charts appear to be rendered correctly.");

  info("Waiting for chart to display");

  await waitUntil(
    () => document.querySelectorAll(".pie-chart-container:not([placeholder=true])")
                  .length == 2);
  ok(true, "Two real pie charts appear to be rendered correctly.");

  await waitUntil(
    () => document.querySelectorAll(".table-chart-container:not([placeholder=true])")
                  .length == 2);
  ok(true, "Two real table charts appear to be rendered correctly.");

  await teardown(monitor);
});
