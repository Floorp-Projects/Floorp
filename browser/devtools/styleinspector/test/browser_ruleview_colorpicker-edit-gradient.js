/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that changing a color in a gradient css declaration using the tooltip
// color picker works

const PAGE_CONTENT = [
  '<style type="text/css">',
  '  body {',
  '    background-image: linear-gradient(to left, #f06 25%, #333 95%, #000 100%);',
  '  }',
  '</style>',
  'Updating a gradient declaration with the color picker tooltip'
].join("\n");

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8,rule view color picker tooltip test");

  info("Creating the test document");
  content.document.body.innerHTML = PAGE_CONTENT;

  info("Opening the rule-view")
  let {toolbox, inspector, view} = yield openRuleView();

  info("Testing that the colors in gradient properties are parsed correctly");
  testColorParsing(view);

  info("Testing that changing one of the colors of a gradient property works");
  yield testPickingNewColor(view);
});

function testColorParsing(view) {
  let ruleEl = getRuleViewProperty(view, "body", "background-image");
  ok(ruleEl, "The background-image gradient declaration was found");

  let swatchEls = ruleEl.valueSpan.querySelectorAll(".ruleview-colorswatch");
  ok(swatchEls, "The color swatch elements were found");
  is(swatchEls.length, 3, "There are 3 color swatches");

  let colorEls = ruleEl.valueSpan.querySelectorAll(".ruleview-color");
  ok(colorEls, "The color elements were found");
  is(colorEls.length, 3, "There are 3 color values");

  let colors = ["#F06", "#333", "#000"];
  for (let i = 0; i < colors.length; i ++) {
    is(colorEls[i].textContent, colors[i], "The right color value was found");
  }
}

function* testPickingNewColor(view) {
  // Grab the first color swatch and color in the gradient
  let ruleEl = getRuleViewProperty(view, "body", "background-image");
  let swatchEl = ruleEl.valueSpan.querySelector(".ruleview-colorswatch");
  let colorEl = ruleEl.valueSpan.querySelector(".ruleview-color");

  info("Getting the color picker tooltip and clicking on the swatch to show it");
  let cPicker = view.tooltips.colorPicker;
  let onShown = cPicker.tooltip.once("shown");
  swatchEl.click();
  yield onShown;

  yield simulateColorPickerChange(view, cPicker, [1, 1, 1, 1]);

  is(swatchEl.style.backgroundColor, "rgb(1, 1, 1)",
    "The color swatch's background was updated");
  is(colorEl.textContent, "#010101", "The color text was updated");
  is(content.getComputedStyle(content.document.body).backgroundImage,
    "linear-gradient(to left, rgb(1, 1, 1) 25%, rgb(51, 51, 51) 95%, rgb(0, 0, 0) 100%)",
    "The gradient has been updated correctly");

  cPicker.hide();
}
