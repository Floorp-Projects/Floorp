/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that color pickers appear when clicking or using keyboard on color swatches.

const TEST_URI = `
  <style type="text/css">
    body {
      color: red;
      background-color: #ededed;
      background-image: url(chrome://branding/content/icon64.png);
      border: 2em solid rgba(120, 120, 120, .5);
    }
  </style>
  Testing the color picker tooltip!
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { view } = await openRuleView();

  const propertiesToTest = ["color", "background-color", "border"];

  for (const property of propertiesToTest) {
    info(`Test that the colorpicker appears on swatch click for ${property}`);
    await testColorPickerAppearsOnColorSwatchActivation(view, property);

    info(
      `Test that swatch is focusable and colorpicker can be activated with a keyboard for ${property}`
    );
    await testColorPickerAppearsOnColorSwatchActivation(view, property, true);
  }
});

async function testColorPickerAppearsOnColorSwatchActivation(
  view,
  property,
  withKeyboard = false
) {
  const value = getRuleViewProperty(view, "body", property).valueSpan;
  const swatch = value.querySelector(".ruleview-colorswatch");

  const cPicker = view.tooltips.getTooltip("colorPicker");
  ok(cPicker, "The rule-view has an expected colorPicker widget");

  const cPickerPanel = cPicker.tooltip.panel;
  ok(cPickerPanel, "The XUL panel for the color picker exists");

  const onColorPickerReady = cPicker.once("ready");
  if (withKeyboard) {
    // Focus on the property value span
    const doc = value.ownerDocument;
    value.focus();

    // Tab to focus on the color swatch
    EventUtils.sendKey("Tab");
    is(doc.activeElement, swatch, "Swatch successfully receives focus.");

    // Press enter on the swatch to simulate click and open color picker
    EventUtils.sendKey("Return");
  } else {
    swatch.click();
  }
  await onColorPickerReady;

  info("The color picker was displayed");
  ok(!inplaceEditor(swatch.parentNode), "The inplace editor wasn't displayed");

  await hideTooltipAndWaitForRuleViewChanged(cPicker, view);
}
