/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test cycling angle units in the rule view.

const TEST_URI = `
  <style type="text/css">
    body {
      image-orientation: 1turn;
    }
    div {
      image-orientation: 180deg;
    }
  </style>
  <body><div>Test</div>cycling angle units in the rule view!</body>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  let container = getRuleViewProperty(
    view, "body", "image-orientation").valueSpan;
  yield checkAngleCycling(container, view);
  yield checkAngleCyclingPersist(inspector, view);
});

function* checkAngleCycling(container, view) {
  let valueNode = container.querySelector(".ruleview-angle");
  let win = view.styleWindow;

  // turn
  is(valueNode.textContent, "1turn", "Angle displayed as a turn value.");

  let tests = [{
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

  for (let test of tests) {
    yield checkSwatchShiftClick(container, win, test.value, test.comment);
  }
}

function* checkAngleCyclingPersist(inspector, view) {
  yield selectNode("div", inspector);
  let container = getRuleViewProperty(
    view, "div", "image-orientation").valueSpan;
  let valueNode = container.querySelector(".ruleview-angle");
  let win = view.styleWindow;

  is(valueNode.textContent, "180deg", "Angle displayed as a degree value.");

  yield checkSwatchShiftClick(container, win,
    `${Math.round(Math.PI * 10000) / 10000}rad`,
    "Angle displayed as a radian value.");

  // Select the body and reselect the div to see
  // if the new angle unit persisted
  yield selectNode("body", inspector);
  yield selectNode("div", inspector);

  // We have to query for the container and the swatch because
  // they've been re-generated
  container = getRuleViewProperty(view, "div", "image-orientation").valueSpan;
  valueNode = container.querySelector(".ruleview-angle");
  is(valueNode.textContent, `${Math.round(Math.PI * 10000) / 10000}rad`,
    "Angle still displayed as a radian value.");
}

function* checkSwatchShiftClick(container, win, expectedValue, comment) {
  let swatch = container.querySelector(".ruleview-angleswatch");
  let valueNode = container.querySelector(".ruleview-angle");

  let onUnitChange = swatch.once("unit-change");
  EventUtils.synthesizeMouseAtCenter(swatch, {
    type: "mousedown",
    shiftKey: true
  }, win);
  yield onUnitChange;
  is(valueNode.textContent, expectedValue, comment);
}
