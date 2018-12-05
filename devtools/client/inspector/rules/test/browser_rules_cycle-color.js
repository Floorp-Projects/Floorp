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
  const {inspector, view} = await openRuleView();
  const container = getRuleViewProperty(view, "body", "color").valueSpan;
  await checkColorCycling(container, view);
  await checkAlphaColorCycling(inspector, view);
  await checkColorCyclingPersist(inspector, view);
  await checkColorCyclingWithDifferentDefaultType(inspector, view);
});

async function checkColorCycling(container, view) {
  const valueNode = container.querySelector(".ruleview-color");
  const win = view.styleWindow;

  // Hex
  is(valueNode.textContent, "#f00", "Color displayed as a hex value.");

  const tests = [{
    value: "hsl(0, 100%, 50%)",
    comment: "Color displayed as an HSL value.",
  }, {
    value: "rgb(255, 0, 0)",
    comment: "Color displayed as an RGB value.",
  }, {
    value: "red",
    comment: "Color displayed as a color name.",
  }, {
    value: "#f00",
    comment: "Color displayed as an authored value.",
  }, {
    value: "hsl(0, 100%, 50%)",
    comment: "Color displayed as an HSL value again.",
  }];

  for (const test of tests) {
    await checkSwatchShiftClick(view, container, win, test.value, test.comment);
  }
}

async function checkAlphaColorCycling(inspector, view) {
  await selectNode("span", inspector);
  const container = getRuleViewProperty(view, "span", "border-color").valueSpan;
  const valueNode = container.querySelector(".ruleview-color");
  const win = view.styleWindow;

  is(valueNode.textContent, "#ff000080",
    "Color displayed as an alpha hex value.");

  const tests = [{
    value: "hsla(0, 100%, 50%, 0.5)",
    comment: "Color displayed as an HSLa value.",
  }, {
    value: "rgba(255, 0, 0, 0.5)",
    comment: "Color displayed as an RGBa value.",
  }, {
    value: "#ff000080",
    comment: "Color displayed as an alpha hex value again.",
  }];

  for (const test of tests) {
    await checkSwatchShiftClick(view, container, win, test.value, test.comment);
  }
}

async function checkColorCyclingPersist(inspector, view) {
  await selectNode("span", inspector);
  let container = getRuleViewProperty(view, "span", "color").valueSpan;
  let valueNode = container.querySelector(".ruleview-color");
  const win = view.styleWindow;

  is(valueNode.textContent, "blue", "Color displayed as a color name.");

  await checkSwatchShiftClick(view, container, win, "#00f",
    "Color displayed as a hex value.");

  // Select the body and reselect the span to see
  // if the new color unit persisted
  await selectNode("body", inspector);
  await selectNode("span", inspector);

  // We have to query for the container and the swatch because
  // they've been re-generated
  container = getRuleViewProperty(view, "span", "color").valueSpan;
  valueNode = container.querySelector(".ruleview-color");
  is(valueNode.textContent, "#00f",
    "Color  is still displayed as a hex value.");
}

async function checkColorCyclingWithDifferentDefaultType(inspector, view) {
  info("Change the default color type pref to hex");
  await pushPref("devtools.defaultColorUnit", "hex");

  info("Select a new node that would normally have a color with a different type");
  await selectNode("div", inspector);

  let container = getRuleViewProperty(view, "div", "color").valueSpan;
  let valueNode = container.querySelector(".ruleview-color");
  const win = view.styleWindow;

  is(valueNode.textContent, "#008000", "Color displayed as a hex value.");

  info("Cycle through color types again");

  const tests = [{
    value: "hsl(120, 100%, 25.1%)",
    comment: "Color displayed as an HSL value.",
  }, {
    value: "rgb(0, 128, 0)",
    comment: "Color displayed as an RGB value.",
  }, {
    value: "green",
    comment: "Color displayed as a color name.",
  }, {
    value: "#008000",
    comment: "Color displayed as an authored value.",
  }, {
    value: "hsl(120, 100%, 25.1%)",
    comment: "Color displayed as an HSL value again.",
  }];

  for (const test of tests) {
    await checkSwatchShiftClick(view, container, win, test.value, test.comment);
  }
}

async function checkSwatchShiftClick(view, container, win, expectedValue, comment) {
  const swatch = container.querySelector(".ruleview-colorswatch");
  const valueNode = container.querySelector(".ruleview-color");

  const onRuleViewChanged = view.once("ruleview-changed");
  const onUnitChange = swatch.once("unit-change");
  EventUtils.synthesizeMouseAtCenter(swatch, {
    type: "mousedown",
    shiftKey: true,
  }, win);
  await onUnitChange;
  await onRuleViewChanged;
  is(valueNode.textContent, expectedValue, comment);
}
