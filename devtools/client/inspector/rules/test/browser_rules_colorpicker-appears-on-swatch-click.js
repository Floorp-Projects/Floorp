/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that color pickers appear when clicking on color swatches.

const TEST_URI = `
  <style type="text/css">
    body {
      color: red;
      background-color: #ededed;
      background-image: url(chrome://global/skin/icons/warning-64.png);
      border: 2em solid rgba(120, 120, 120, .5);
    }
  </style>
  Testing the color picker tooltip!
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {view} = await openRuleView();

  const propertiesToTest = ["color", "background-color", "border"];

  for (const property of propertiesToTest) {
    info("Testing that the colorpicker appears on swatch click");
    const value = getRuleViewProperty(view, "body", property).valueSpan;
    const swatch = value.querySelector(".ruleview-colorswatch");
    await testColorPickerAppearsOnColorSwatchClick(view, swatch);
  }
});

async function testColorPickerAppearsOnColorSwatchClick(view, swatch) {
  const cPicker = view.tooltips.getTooltip("colorPicker");
  ok(cPicker, "The rule-view has the expected colorPicker property");

  const cPickerPanel = cPicker.tooltip.panel;
  ok(cPickerPanel, "The XUL panel for the color picker exists");

  const onColorPickerReady = cPicker.once("ready");
  swatch.click();
  await onColorPickerReady;

  ok(true, "The color picker was shown on click of the color swatch");
  ok(!inplaceEditor(swatch.parentNode),
    "The inplace editor wasn't shown as a result of the color swatch click");

  await hideTooltipAndWaitForRuleViewChanged(cPicker, view);
}
