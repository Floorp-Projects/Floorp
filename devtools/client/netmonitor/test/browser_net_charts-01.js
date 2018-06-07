/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Makes sure Pie Charts have the right internal structure.
 */

add_task(async function() {
  const { monitor, tab } = await initNetMonitor(SIMPLE_URL);

  info("Starting test... ");

  const { document, windowRequire } = monitor.panelWin;
  const { Chart } = windowRequire("devtools/client/shared/widgets/Chart");

  const wait = waitForNetworkEvents(monitor, 1);
  tab.linkedBrowser.loadURI(SIMPLE_URL);
  await wait;

  const pie = Chart.Pie(document, {
    width: 100,
    height: 100,
    data: [{
      size: 1,
      label: "foo"
    }, {
      size: 2,
      label: "bar"
    }, {
      size: 3,
      label: "baz"
    }]
  });

  const node = pie.node;
  const slices = node.querySelectorAll(".pie-chart-slice.chart-colored-blob");
  const labels = node.querySelectorAll(".pie-chart-label");

  ok(node.classList.contains("pie-chart-container") &&
     node.classList.contains("generic-chart-container"),
    "A pie chart container was created successfully.");

  is(slices.length, 3,
    "There should be 3 pie chart slices created.");
  ok(slices[0].getAttribute("d").match(
    /\s*M 50,50 L 49\.\d+,97\.\d+ A 47\.5,47\.5 0 0 1 49\.\d+,2\.5\d* Z/),
    "The first slice has the correct data.");
  ok(slices[1].getAttribute("d").match(
    /\s*M 50,50 L 91\.\d+,26\.\d+ A 47\.5,47\.5 0 0 1 49\.\d+,97\.\d+ Z/),
    "The second slice has the correct data.");
  ok(slices[2].getAttribute("d").match(
    /\s*M 50,50 L 50\.\d+,2\.5\d* A 47\.5,47\.5 0 0 1 91\.\d+,26\.\d+ Z/),
    "The third slice has the correct data.");

  ok(slices[0].hasAttribute("largest"),
    "The first slice should be the largest one.");
  ok(slices[2].hasAttribute("smallest"),
    "The third slice should be the smallest one.");

  is(slices[0].getAttribute("name"), "baz",
    "The first slice's name is correct.");
  is(slices[1].getAttribute("name"), "bar",
    "The first slice's name is correct.");
  is(slices[2].getAttribute("name"), "foo",
    "The first slice's name is correct.");

  is(labels.length, 3,
    "There should be 3 pie chart labels created.");
  is(labels[0].textContent, "baz",
    "The first label's text is correct.");
  is(labels[1].textContent, "bar",
    "The first label's text is correct.");
  is(labels[2].textContent, "foo",
    "The first label's text is correct.");

  await teardown(monitor);
});
