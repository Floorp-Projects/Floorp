/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test cycling angle units in the rule view.

const TEST_URI = `
  <style type="text/css">
    body {
      filter: hue-rotate(1turn);
    }
    div {
      filter: hue-rotate(180deg);
    }
  </style>
  <body><div>Test</div>cycling angle units in the rule view!</body>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();
  const container = getRuleViewProperty(view, "body", "filter").valueSpan;
  await checkAngleCycling(container, view);
  await checkAngleCyclingPersist(inspector, view);
});

async function checkAngleCycling(container, view) {
  const valueNode = container.querySelector(".ruleview-angle");
  const win = view.styleWindow;

  // turn
  is(valueNode.textContent, "1turn", "Angle displayed as a turn value.");

  const tests = [{
    value: "360deg",
    comment: "Angle displayed as a degree value."
  }, {
    value: `${Math.round(Math.PI * 2 * 10000) / 10000}rad`,
    comment: "Angle displayed as a radian value."
  }, {
    value: "400grad",
    comment: "Angle displayed as a gradian value."
  }, {
    value: "1turn",
    comment: "Angle displayed as a turn value again."
  }];

  for (const test of tests) {
    await checkSwatchShiftClick(container, win, test.value, test.comment);
  }
}

async function checkAngleCyclingPersist(inspector, view) {
  await selectNode("div", inspector);
  let container = getRuleViewProperty(view, "div", "filter").valueSpan;
  let valueNode = container.querySelector(".ruleview-angle");
  const win = view.styleWindow;

  is(valueNode.textContent, "180deg", "Angle displayed as a degree value.");

  await checkSwatchShiftClick(container, win,
    `${Math.round(Math.PI * 10000) / 10000}rad`,
    "Angle displayed as a radian value.");

  // Select the body and reselect the div to see
  // if the new angle unit persisted
  await selectNode("body", inspector);
  await selectNode("div", inspector);

  // We have to query for the container and the swatch because
  // they've been re-generated
  container = getRuleViewProperty(view, "div", "filter").valueSpan;
  valueNode = container.querySelector(".ruleview-angle");
  is(valueNode.textContent, `${Math.round(Math.PI * 10000) / 10000}rad`,
    "Angle still displayed as a radian value.");
}

async function checkSwatchShiftClick(container, win, expectedValue, comment) {
  const swatch = container.querySelector(".ruleview-angleswatch");
  const valueNode = container.querySelector(".ruleview-angle");

  const onUnitChange = swatch.once("unit-change");
  EventUtils.synthesizeMouseAtCenter(swatch, {
    type: "mousedown",
    shiftKey: true
  }, win);
  await onUnitChange;
  is(valueNode.textContent, expectedValue, comment);
}
