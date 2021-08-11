/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that after a color change, opening another tooltip, like the image
// preview doesn't revert the color change in the rule view.
// This used to happen when the activeSwatch wasn't reset when the colorpicker
// would hide.
// See bug 979292

const TEST_URI = `
  <style type="text/css">
    body {
      background: red url("chrome://branding/content/icon64.png")
        no-repeat center center;
    }
  </style>
  Testing the color picker tooltip!
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { view } = await openRuleView();
  await testColorChangeIsntRevertedWhenOtherTooltipIsShown(view);
});

async function testColorChangeIsntRevertedWhenOtherTooltipIsShown(ruleView) {
  let swatch = getRuleViewProperty(
    ruleView,
    "body",
    "background"
  ).valueSpan.querySelector(".ruleview-colorswatch");

  info("Open the color picker tooltip and change the color");
  const picker = ruleView.tooltips.getTooltip("colorPicker");
  const onColorPickerReady = picker.once("ready");
  swatch.click();
  await onColorPickerReady;

  await simulateColorPickerChange(ruleView, picker, [0, 0, 0, 1], {
    selector: "body",
    name: "background-color",
    value: "rgb(0, 0, 0)",
  });

  const spectrum = picker.spectrum;

  const onModifications = waitForNEvents(ruleView, "ruleview-changed", 2);
  const onHidden = picker.tooltip.once("hidden");
  focusAndSendKey(spectrum.element.ownerDocument.defaultView, "RETURN");
  await onHidden;
  await onModifications;

  info("Open the image preview tooltip");
  const value = getRuleViewProperty(ruleView, "body", "background").valueSpan;
  const url = value.querySelector(".theme-link");
  const previewTooltip = await assertShowPreviewTooltip(ruleView, url);

  info("Image tooltip is shown, verify that the swatch is still correct");
  swatch = value.querySelector(".ruleview-colorswatch");
  is(swatch.style.backgroundColor, "black", "The swatch's color is correct");
  is(swatch.nextSibling.textContent, "black", "The color name is correct");

  await assertTooltipHiddenOnMouseOut(previewTooltip, url);
}
