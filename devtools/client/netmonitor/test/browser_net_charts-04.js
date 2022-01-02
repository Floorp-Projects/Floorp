/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Makes sure Pie Charts have the right internal structure when
 * initialized with empty data.
 */

add_task(async function() {
  const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");

  const { monitor } = await initNetMonitor(HTTPS_SIMPLE_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, windowRequire } = monitor.panelWin;
  const { Chart } = windowRequire("devtools/client/shared/widgets/Chart");

  const wait = waitForNetworkEvents(monitor, 1);
  await navigateTo(HTTPS_SIMPLE_URL);
  await wait;

  const table = Chart.Table(document, {
    title: "Table title",
    data: null,
    totals: {
      label1: value => "Hello " + L10N.numberWithDecimals(value, 2),
      label2: value => "World " + L10N.numberWithDecimals(value, 2),
    },
    header: {
      label1: "",
      label2: "",
    },
  });

  const { node } = table;
  const title = node.querySelector(".table-chart-title");
  const grid = node.querySelector(".table-chart-grid");
  const totals = node.querySelector(".table-chart-totals");
  const rows = grid.querySelectorAll(".table-chart-row");
  const sums = node.querySelectorAll(".table-chart-summary-label");

  ok(
    node.classList.contains("table-chart-container") &&
      node.classList.contains("generic-chart-container"),
    "A table chart container was created successfully."
  );

  ok(title, "A title node was created successfully.");
  is(
    title.textContent,
    "Table title",
    "The title node displays the correct text."
  );

  is(
    rows.length,
    2,
    "There should be 1 table chart row and a 1 header created."
  );

  ok(
    rows[1].querySelector(".table-chart-row-box.chart-colored-blob"),
    "A colored blob exists for the first row."
  );
  is(
    rows[1].querySelectorAll("span")[0].getAttribute("name"),
    "size",
    "The first column of the first row exists."
  );
  is(
    rows[1].querySelectorAll("span")[1].getAttribute("name"),
    "label",
    "The second column of the first row exists."
  );
  is(
    rows[1].querySelectorAll("span")[0].textContent,
    "",
    "The first column of the first row displays the correct text."
  );
  is(
    rows[1].querySelectorAll("span")[1].textContent,
    L10N.getStr("tableChart.loading"),
    "The second column of the first row displays the correct text."
  );

  is(sums.length, 2, "There should be 2 total summaries created.");

  is(
    totals
      .querySelectorAll(".table-chart-summary-label")[0]
      .getAttribute("name"),
    "label1",
    "The first sum's type is correct."
  );
  is(
    totals.querySelectorAll(".table-chart-summary-label")[0].textContent,
    "Hello 0",
    "The first sum's value is correct."
  );

  is(
    totals
      .querySelectorAll(".table-chart-summary-label")[1]
      .getAttribute("name"),
    "label2",
    "The second sum's type is correct."
  );
  is(
    totals.querySelectorAll(".table-chart-summary-label")[1].textContent,
    "World 0",
    "The second sum's value is correct."
  );

  await teardown(monitor);
});
