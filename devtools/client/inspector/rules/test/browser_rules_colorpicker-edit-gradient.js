/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that changing a color in a gradient css declaration using the tooltip
// color picker works.

const TEST_URI = `
  <style type="text/css">
    body {
      background-image: linear-gradient(to left, #f06 25%, #333 95%, #000 100%);
    }
  </style>
  Updating a gradient declaration with the color picker tooltip
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { view } = await openRuleView();

  info("Testing that the colors in gradient properties are parsed correctly");
  testColorParsing(view);

  info("Testing that changing one of the colors of a gradient property works");
  await testPickingNewColor(view);
});

function testColorParsing(view) {
  const ruleEl = getRuleViewProperty(view, "body", "background-image");
  ok(ruleEl, "The background-image gradient declaration was found");

  const swatchEls = ruleEl.valueSpan.querySelectorAll(".ruleview-colorswatch");
  ok(swatchEls, "The color swatch elements were found");
  is(swatchEls.length, 3, "There are 3 color swatches");

  const colorEls = ruleEl.valueSpan.querySelectorAll(".ruleview-color");
  ok(colorEls, "The color elements were found");
  is(colorEls.length, 3, "There are 3 color values");

  const colors = ["#f06", "#333", "#000"];
  for (let i = 0; i < colors.length; i++) {
    is(colorEls[i].textContent, colors[i], "The right color value was found");
  }
}

async function testPickingNewColor(view) {
  // Grab the first color swatch and color in the gradient
  const ruleEl = getRuleViewProperty(view, "body", "background-image");
  const swatchEl = ruleEl.valueSpan.querySelector(".ruleview-colorswatch");
  const colorEl = ruleEl.valueSpan.querySelector(".ruleview-color");

  info("Get the color picker tooltip and clicking on the swatch to show it");
  const cPicker = view.tooltips.getTooltip("colorPicker");
  const onColorPickerReady = cPicker.once("ready");
  swatchEl.click();
  await onColorPickerReady;

  const change = {
    selector: "body",
    name: "background-image",
    value:
      "linear-gradient(to left, rgb(1, 1, 1) 25%, " +
      "rgb(51, 51, 51) 95%, rgb(0, 0, 0) 100%)",
  };
  await simulateColorPickerChange(view, cPicker, [1, 1, 1, 1], change);

  is(
    swatchEl.style.backgroundColor,
    "rgb(1, 1, 1)",
    "The color swatch's background was updated"
  );
  is(colorEl.textContent, "#010101", "The color text was updated");
  is(
    await getComputedStyleProperty("body", null, "background-image"),
    "linear-gradient(to left, rgb(1, 1, 1) 25%, rgb(51, 51, 51) 95%, " +
      "rgb(0, 0, 0) 100%)",
    "The gradient has been updated correctly"
  );

  await hideTooltipAndWaitForRuleViewChanged(cPicker, view);
}
