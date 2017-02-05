/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Makes sure Table Charts have the right internal structure.
 */

add_task(function* () {
  let { L10N } = require("devtools/client/netmonitor/l10n");

  let { monitor } = yield initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  let { document, windowRequire } = monitor.panelWin;
  let { Chart } = windowRequire("devtools/client/shared/widgets/Chart");

  let table = Chart.Table(document, {
    title: "Table title",
    data: [{
      label1: 1,
      label2: 11.1
    }, {
      label1: 2,
      label2: 12.2
    }, {
      label1: 3,
      label2: 13.3
    }],
    strings: {
      label2: (value, index) => value + ["foo", "bar", "baz"][index]
    },
    totals: {
      label1: value => "Hello " + L10N.numberWithDecimals(value, 2),
      label2: value => "World " + L10N.numberWithDecimals(value, 2)
    }
  });

  let node = table.node;
  let title = node.querySelector(".table-chart-title");
  let grid = node.querySelector(".table-chart-grid");
  let totals = node.querySelector(".table-chart-totals");
  let rows = grid.querySelectorAll(".table-chart-row");
  let sums = node.querySelectorAll(".table-chart-summary-label");

  ok(node.classList.contains("table-chart-container") &&
     node.classList.contains("generic-chart-container"),
    "A table chart container was created successfully.");

  ok(title, "A title node was created successfully.");
  is(title.textContent, "Table title",
    "The title node displays the correct text.");

  is(rows.length, 3, "There should be 3 table chart rows created.");

  ok(rows[0].querySelector(".table-chart-row-box.chart-colored-blob"),
    "A colored blob exists for the firt row.");
  is(rows[0].querySelectorAll("span")[0].getAttribute("name"), "label1",
    "The first column of the first row exists.");
  is(rows[0].querySelectorAll("span")[1].getAttribute("name"), "label2",
    "The second column of the first row exists.");
  is(rows[0].querySelectorAll("span")[0].textContent, "1",
    "The first column of the first row displays the correct text.");
  is(rows[0].querySelectorAll("span")[1].textContent, "11.1foo",
    "The second column of the first row displays the correct text.");

  ok(rows[1].querySelector(".table-chart-row-box.chart-colored-blob"),
    "A colored blob exists for the second row.");
  is(rows[1].querySelectorAll("span")[0].getAttribute("name"), "label1",
    "The first column of the second row exists.");
  is(rows[1].querySelectorAll("span")[1].getAttribute("name"), "label2",
    "The second column of the second row exists.");
  is(rows[1].querySelectorAll("span")[0].textContent, "2",
    "The first column of the second row displays the correct text.");
  is(rows[1].querySelectorAll("span")[1].textContent, "12.2bar",
    "The second column of the first row displays the correct text.");

  ok(rows[2].querySelector(".table-chart-row-box.chart-colored-blob"),
    "A colored blob exists for the third row.");
  is(rows[2].querySelectorAll("span")[0].getAttribute("name"), "label1",
    "The first column of the third row exists.");
  is(rows[2].querySelectorAll("span")[1].getAttribute("name"), "label2",
    "The second column of the third row exists.");
  is(rows[2].querySelectorAll("span")[0].textContent, "3",
    "The first column of the third row displays the correct text.");
  is(rows[2].querySelectorAll("span")[1].textContent, "13.3baz",
    "The second column of the third row displays the correct text.");

  is(sums.length, 2, "There should be 2 total summaries created.");

  is(totals.querySelectorAll(".table-chart-summary-label")[0].getAttribute("name"),
    "label1",
    "The first sum's type is correct.");
  is(totals.querySelectorAll(".table-chart-summary-label")[0].textContent,
    "Hello 6",
    "The first sum's value is correct.");

  is(totals.querySelectorAll(".table-chart-summary-label")[1].getAttribute("name"),
    "label2",
    "The second sum's type is correct.");
  is(totals.querySelectorAll(".table-chart-summary-label")[1].textContent,
    "World 36.60",
    "The second sum's value is correct.");

  yield teardown(monitor);
});
