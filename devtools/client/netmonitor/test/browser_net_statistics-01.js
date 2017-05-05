/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the statistics panel displays correctly.
 */

add_task(function* () {
  let { monitor } = yield initNetMonitor(STATISTICS_URL);
  info("Starting test... ");

  let panel = monitor.panelWin;
  let { document, store, windowRequire } = panel;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  ok(document.querySelector(".monitor-panel"),
    "The current main panel is correct.");

  info("Displaying statistics panel");
  store.dispatch(Actions.openStatistics(true));

  ok(document.querySelector(".statistics-panel"),
    "The current main panel is correct.");

  info("Waiting for placeholder to display");

  yield waitUntil(
    () => document.querySelectorAll(".pie-chart-container[placeholder=true]")
                  .length == 2);
  ok(true, "Two placeholder pie charts appear to be rendered correctly.");

  yield waitUntil(
    () => document.querySelectorAll(".table-chart-container[placeholder=true]")
                  .length == 2);
  ok(true, "Two placeholde table charts appear to be rendered correctly.");

  info("Waiting for chart to display");

  yield waitUntil(
    () => document.querySelectorAll(".pie-chart-container:not([placeholder=true])")
                  .length == 2);
  ok(true, "Two real pie charts appear to be rendered correctly.");

  yield waitUntil(
    () => document.querySelectorAll(".table-chart-container:not([placeholder=true])")
                  .length == 2);
  ok(true, "Two real table charts appear to be rendered correctly.");

  yield teardown(monitor);
});
