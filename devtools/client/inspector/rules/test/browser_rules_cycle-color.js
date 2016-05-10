/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test cycling color types in the rule view.

const TEST_URI = `
  <style type="text/css">
    body {
      color: #f00;
    }
    span {
      color: blue;
      border-color: #ff000080;
    }
  </style>
  <body><span>Test</span> cycling color types in the rule view!</body>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  let container = getRuleViewProperty(view, "body", "color").valueSpan;
  yield checkColorCycling(container, view);
  yield checkAlphaColorCycling(inspector, view);
  yield checkColorCyclingPersist(inspector, view);
});

function* checkColorCycling(container, view) {
  let valueNode = container.querySelector(".ruleview-color");
  let win = view.styleWindow;

  // Hex
  is(valueNode.textContent, "#f00", "Color displayed as a hex value.");

  let tests = [{
    value: "hsl(0, 100%, 50%)",
    comment: "Color displayed as an HSL value."
  }, {
    value: "rgb(255, 0, 0)",
    comment: "Color displayed as an RGB value."
  }, {
    value: "red",
    comment: "Color displayed as a color name."
  }, {
    value: "#f00",
    comment: "Color displayed as an authored value."
  }, {
    value: "hsl(0, 100%, 50%)",
    comment: "Color displayed as an HSL value again."
  }];

  for (let test of tests) {
    yield checkSwatchShiftClick(container, win, test.value, test.comment);
  }
}

function* checkAlphaColorCycling(inspector, view) {
  yield selectNode("span", inspector);
  let container = getRuleViewProperty(view, "span", "border-color").valueSpan;
  let valueNode = container.querySelector(".ruleview-color");
  let win = view.styleWindow;

  is(valueNode.textContent, "#ff000080",
    "Color displayed as an alpha hex value.");

  let tests = [{
    value: "hsla(0, 100%, 50%, 0.5)",
    comment: "Color displayed as an HSLa value."
  }, {
    value: "rgba(255, 0, 0, 0.5)",
    comment: "Color displayed as an RGBa value."
  }, {
    value: "#ff000080",
    comment: "Color displayed as an alpha hex value again."
  }];

  for (let test of tests) {
    yield checkSwatchShiftClick(container, win, test.value, test.comment);
  }
}

function* checkColorCyclingPersist(inspector, view) {
  yield selectNode("span", inspector);
  let container = getRuleViewProperty(view, "span", "color").valueSpan;
  let valueNode = container.querySelector(".ruleview-color");
  let win = view.styleWindow;

  is(valueNode.textContent, "blue", "Color displayed as a color name.");

  yield checkSwatchShiftClick(container, win, "#00f",
    "Color displayed as a hex value.");

  // Select the body and reselect the span to see
  // if the new color unit persisted
  yield selectNode("body", inspector);
  yield selectNode("span", inspector);

  // We have to query for the container and the swatch because
  // they've been re-generated
  container = getRuleViewProperty(view, "span", "color").valueSpan;
  valueNode = container.querySelector(".ruleview-color");
  is(valueNode.textContent, "#00f",
    "Color  is still displayed as a hex value.");
}

function* checkSwatchShiftClick(container, win, expectedValue, comment) {
  let swatch = container.querySelector(".ruleview-colorswatch");
  let valueNode = container.querySelector(".ruleview-color");

  let onUnitChange = swatch.once("unit-change");
  EventUtils.synthesizeMouseAtCenter(swatch, {
    type: "mousedown",
    shiftKey: true
  }, win);
  yield onUnitChange;
  is(valueNode.textContent, expectedValue, comment);
}
