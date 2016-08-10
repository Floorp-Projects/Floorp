/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the color in the colorpicker tooltip can be changed several times.
// without causing error in various cases:
// - simple single-color property (color)
// - color and image property (background-image)
// - overridden property
// See bug 979292 and bug 980225

const TEST_URI = `
  <style type="text/css">
    body {
      color: green;
      background: red url("chrome://global/skin/icons/warning-64.png")
        no-repeat center center;
    }
    p {
      color: blue;
    }
  </style>
  <p>Testing the color picker tooltip!</p>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();

  yield testSimpleMultipleColorChanges(inspector, view);
  yield testComplexMultipleColorChanges(inspector, view);
  yield testOverriddenMultipleColorChanges(inspector, view);
});

function* testSimpleMultipleColorChanges(inspector, ruleView) {
  yield selectNode("p", inspector);

  info("Getting the <p> tag's color property");
  let swatch = getRuleViewProperty(ruleView, "p", "color").valueSpan
    .querySelector(".ruleview-colorswatch");

  info("Opening the color picker");
  let picker = ruleView.tooltips.colorPicker;
  let onColorPickerReady = picker.once("ready");
  swatch.click();
  yield onColorPickerReady;

  info("Changing the color several times");
  let colors = [
    {rgba: [0, 0, 0, 1], computed: "rgb(0, 0, 0)"},
    {rgba: [100, 100, 100, 1], computed: "rgb(100, 100, 100)"},
    {rgba: [200, 200, 200, 1], computed: "rgb(200, 200, 200)"}
  ];
  for (let {rgba, computed} of colors) {
    yield simulateColorPickerChange(ruleView, picker, rgba, {
      selector: "p",
      name: "color",
      value: computed
    });
  }
}

function* testComplexMultipleColorChanges(inspector, ruleView) {
  yield selectNode("body", inspector);

  info("Getting the <body> tag's color property");
  let swatch = getRuleViewProperty(ruleView, "body", "background").valueSpan
    .querySelector(".ruleview-colorswatch");

  info("Opening the color picker");
  let picker = ruleView.tooltips.colorPicker;
  let onColorPickerReady = picker.once("ready");
  swatch.click();
  yield onColorPickerReady;

  info("Changing the color several times");
  let colors = [
    {rgba: [0, 0, 0, 1], computed: "rgb(0, 0, 0)"},
    {rgba: [100, 100, 100, 1], computed: "rgb(100, 100, 100)"},
    {rgba: [200, 200, 200, 1], computed: "rgb(200, 200, 200)"}
  ];
  for (let {rgba, computed} of colors) {
    yield simulateColorPickerChange(ruleView, picker, rgba, {
      selector: "body",
      name: "background-color",
      value: computed
    });
  }

  info("Closing the color picker");
  yield hideTooltipAndWaitForRuleViewChanged(picker, ruleView);
}

function* testOverriddenMultipleColorChanges(inspector, ruleView) {
  yield selectNode("p", inspector);

  info("Getting the <body> tag's color property");
  let swatch = getRuleViewProperty(ruleView, "body", "color").valueSpan
    .querySelector(".ruleview-colorswatch");

  info("Opening the color picker");
  let picker = ruleView.tooltips.colorPicker;
  let onColorPickerReady = picker.once("ready");
  swatch.click();
  yield onColorPickerReady;

  info("Changing the color several times");
  let colors = [
    {rgba: [0, 0, 0, 1], computed: "rgb(0, 0, 0)"},
    {rgba: [100, 100, 100, 1], computed: "rgb(100, 100, 100)"},
    {rgba: [200, 200, 200, 1], computed: "rgb(200, 200, 200)"}
  ];
  for (let {rgba, computed} of colors) {
    yield simulateColorPickerChange(ruleView, picker, rgba, {
      selector: "body",
      name: "color",
      value: computed
    });
    is((yield getComputedStyleProperty("p", null, "color")),
      "rgb(200, 200, 200)", "The color of the P tag is still correct");
  }
}
