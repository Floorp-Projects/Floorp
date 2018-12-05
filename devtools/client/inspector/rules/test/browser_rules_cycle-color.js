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
    div {
      color: green;
    }
  </style>
  <body>
    <span>Test</span>
    <div>cycling color types in the rule view!</div>
  </body>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();

  await checkColorCycling(view);
  await checkAlphaColorCycling(inspector, view);
  await checkColorCyclingPersist(inspector, view);
  await checkColorCyclingWithDifferentDefaultType(inspector, view);
});

async function checkColorCycling(view) {
  const { valueSpan } = getRuleViewProperty(view, "body", "color");

  checkColorValue(valueSpan, "#f00", "Color displayed as a hex value, its authored type");

  await runSwatchShiftClickTests(view, valueSpan, [{
    value: "hsl(0, 100%, 50%)",
    comment: "Color displayed as an HSL value",
  }, {
    value: "rgb(255, 0, 0)",
    comment: "Color displayed as an RGB value",
  }, {
    value: "red",
    comment: "Color displayed as a color name",
  }, {
    value: "#f00",
    comment: "Color displayed as an authored value",
  }, {
    value: "hsl(0, 100%, 50%)",
    comment: "Color displayed as an HSL value again",
  }]);
}

async function checkAlphaColorCycling(inspector, view) {
  await selectNode("span", inspector);
  const { valueSpan } = getRuleViewProperty(view, "span", "border-color");

  checkColorValue(valueSpan, "#ff000080",
    "Color displayed as an alpha hex value, its authored type");

  await runSwatchShiftClickTests(view, valueSpan, [{
    value: "hsla(0, 100%, 50%, 0.5)",
    comment: "Color displayed as an HSLa value",
  }, {
    value: "rgba(255, 0, 0, 0.5)",
    comment: "Color displayed as an RGBa value",
  }, {
    value: "#ff000080",
    comment: "Color displayed as an alpha hex value again",
  }]);
}

async function checkColorCyclingPersist(inspector, view) {
  await selectNode("span", inspector);
  let { valueSpan } = getRuleViewProperty(view, "span", "color");

  checkColorValue(valueSpan, "blue", "Color displayed as color name, its authored type");

  await checkSwatchShiftClick(view, valueSpan, "#00f", "Color displayed as a hex value");

  info("Select the body and reselect the span to see if the new color unit persisted");
  await selectNode("body", inspector);
  await selectNode("span", inspector);

  // We have to query for the value span and the swatch again because they've been
  // re-generated.
  ({ valueSpan } = getRuleViewProperty(view, "span", "color"));
  checkColorValue(valueSpan, "#00f", "Color is still displayed as a hex value");
}

async function checkColorCyclingWithDifferentDefaultType(inspector, view) {
  info("Change the default color type pref to hex");
  await pushPref("devtools.defaultColorUnit", "hex");

  info("Select a new node that would normally have a color with a different type");
  await selectNode("div", inspector);
  const { valueSpan } = getRuleViewProperty(view, "div", "color");

  checkColorValue(valueSpan, "#008000",
    "Color displayed as a hex value, which is the type just selected");

  info("Cycle through color types again");
  await runSwatchShiftClickTests(view, valueSpan, [{
    value: "hsl(120, 100%, 25.1%)",
    comment: "Color displayed as an HSL value",
  }, {
    value: "rgb(0, 128, 0)",
    comment: "Color displayed as an RGB value",
  }, {
    value: "green",
    comment: "Color displayed as a color name",
  }, {
    value: "#008000",
    comment: "Color displayed as an authored value",
  }, {
    value: "hsl(120, 100%, 25.1%)",
    comment: "Color displayed as an HSL value again",
  }]);
}

async function runSwatchShiftClickTests(view, valueSpan, tests) {
  for (const { value, comment } of tests) {
    await checkSwatchShiftClick(view, valueSpan, value, comment);
  }
}

async function checkSwatchShiftClick(view, valueSpan, expectedValue, comment) {
  const swatchNode = valueSpan.querySelector(".ruleview-colorswatch");
  const colorNode = valueSpan.querySelector(".ruleview-color");

  info("Shift-click the color swatch and wait for the color type and ruleview to update");
  const onRuleViewChanged = view.once("ruleview-changed");
  const onUnitChange = swatchNode.once("unit-change");

  EventUtils.synthesizeMouseAtCenter(swatchNode, {
    type: "mousedown",
    shiftKey: true,
  }, view.styleWindow);

  await onUnitChange;
  await onRuleViewChanged;

  is(colorNode.textContent, expectedValue, comment);
}

function checkColorValue(valueSpan, expectedColorValue, comment) {
  const colorNode = valueSpan.querySelector(".ruleview-color");
  is(colorNode.textContent, expectedColorValue, comment);
}
