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
  let { document, gStore, windowRequire } = panel;
  let Actions = windowRequire("devtools/client/netmonitor/actions/index");

  let body = document.querySelector("#body");

  is(body.selectedPanel.id, "inspector-panel",
    "The current main panel is correct.");

  info("Displaying statistics view");
  gStore.dispatch(Actions.openStatistics(true));
  is(body.selectedPanel.id, "statistics-panel",
    "The current main panel is correct.");

  info("Waiting for placeholder to display");

  is(document.querySelector(".primed-cache-chart").childNodes.length, 1,
    "There should be a placeholder primed cache chart created now.");
  is(document.querySelector(".empty-cache-chart").childNodes.length, 1,
    "There should be a placeholder empty cache chart created now.");

  is(document.querySelectorAll(".pie-chart-container[placeholder=true]").length, 2,
    "Two placeholder pie chart appear to be rendered correctly.");
  is(document.querySelectorAll(".table-chart-container[placeholder=true]").length, 2,
    "Two placeholder table chart appear to be rendered correctly.");

  info("Waiting for chart to display");

  is(document.querySelector(".primed-cache-chart").childNodes.length, 1,
    "There should be a real primed cache chart created now.");
  is(document.querySelector(".empty-cache-chart").childNodes.length, 1,
    "There should be a real empty cache chart created now.");

  yield waitUntil(
    () => document.querySelectorAll(".pie-chart-container:not([placeholder=true])").length == 2);
  ok(true, "Two real pie charts appear to be rendered correctly.");

  yield waitUntil(
    () => document.querySelectorAll(".table-chart-container:not([placeholder=true])").length == 2);
  ok(true, "Two real table charts appear to be rendered correctly.");

  yield teardown(monitor);
});
