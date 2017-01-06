/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Makes sure Table Charts correctly handle empty source data.
 */

add_task(function* () {
  let { L10N } = require("devtools/client/netmonitor/l10n");

  let { monitor } = yield initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  let { document, NetMonitorView } = monitor.panelWin;
  let { Chart } = NetMonitorView.Statistics;

  let table = Chart.Table(document, {
    data: [],
    totals: {
      label1: value => "Hello " + L10N.numberWithDecimals(value, 2),
      label2: value => "World " + L10N.numberWithDecimals(value, 2)
    }
  });

  let node = table.node;
  let grid = node.querySelector(".table-chart-grid");
  let totals = node.querySelector(".table-chart-totals");
  let rows = grid.querySelectorAll(".table-chart-row");
  let sums = node.querySelectorAll(".table-chart-summary-label");

  is(rows.length, 1, "There should be 1 table chart row created.");

  ok(rows[0].querySelector(".table-chart-row-box.chart-colored-blob"),
    "A colored blob exists for the firt row.");
  is(rows[0].querySelectorAll("span")[0].getAttribute("name"), "size",
    "The first column of the first row exists.");
  is(rows[0].querySelectorAll("span")[1].getAttribute("name"), "label",
    "The second column of the first row exists.");
  is(rows[0].querySelectorAll("span")[0].textContent, "",
    "The first column of the first row displays the correct text.");
  is(rows[0].querySelectorAll("span")[1].textContent,
    L10N.getStr("tableChart.unavailable"),
    "The second column of the first row displays the correct text.");

  is(sums.length, 2, "There should be 2 total summaries created.");

  is(totals.querySelectorAll(".table-chart-summary-label")[0].getAttribute("name"),
    "label1",
    "The first sum's type is correct.");
  is(totals.querySelectorAll(".table-chart-summary-label")[0].textContent,
    "Hello 0",
    "The first sum's value is correct.");

  is(totals.querySelectorAll(".table-chart-summary-label")[1].getAttribute("name"),
    "label2",
    "The second sum's type is correct.");
  is(totals.querySelectorAll(".table-chart-summary-label")[1].textContent,
    "World 0",
    "The second sum's value is correct.");

  yield teardown(monitor);
});
