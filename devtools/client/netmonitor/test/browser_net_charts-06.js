/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Makes sure Pie Charts correctly handle empty source data.
 */

add_task(function* () {
  let [, , monitor] = yield initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  let { document, L10N, Chart } = monitor.panelWin;

  let pie = Chart.Pie(document, {
    data: [],
    width: 100,
    height: 100
  });

  let node = pie.node;
  let slices = node.querySelectorAll(".pie-chart-slice.chart-colored-blob");
  let labels = node.querySelectorAll(".pie-chart-label");

  is(slices.length, 1,
    "There should be 1 pie chart slice created.");
  ok(slices[0].getAttribute("d").match(
    /\s*M 50,50 L 50\.\d+,2\.5\d* A 47\.5,47\.5 0 1 1 49\.\d+,2\.5\d* Z/),
    "The slice has the correct data.");

  ok(slices[0].hasAttribute("largest"),
    "The slice should be the largest one.");
  ok(slices[0].hasAttribute("smallest"),
    "The slice should also be the smallest one.");
  ok(slices[0].getAttribute("name"), L10N.getStr("pieChart.unavailable"),
    "The slice's name is correct.");

  is(labels.length, 1,
    "There should be 1 pie chart label created.");
  is(labels[0].textContent, "Empty",
    "The label's text is correct.");

  yield teardown(monitor);
});
