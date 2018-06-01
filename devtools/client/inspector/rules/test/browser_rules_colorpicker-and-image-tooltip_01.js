/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that after a color change, the image preview tooltip in the same
// property is displayed and positioned correctly.
// See bug 979292

const TEST_URI = `
  <style type="text/css">
    body {
      background: url("chrome://global/skin/icons/warning-64.png"), linear-gradient(white, #F06 400px);
    }
  </style>
  Testing the color picker tooltip!
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {view} = await openRuleView();
  const value = getRuleViewProperty(view, "body", "background").valueSpan;
  const swatch = value.querySelectorAll(".ruleview-colorswatch")[0];
  const url = value.querySelector(".theme-link");
  await testImageTooltipAfterColorChange(swatch, url, view);
});

async function testImageTooltipAfterColorChange(swatch, url, ruleView) {
  info("First, verify that the image preview tooltip works");
  let previewTooltip = await assertShowPreviewTooltip(ruleView, url);
  await assertTooltipHiddenOnMouseOut(previewTooltip, url);

  info("Open the color picker tooltip and change the color");
  const picker = ruleView.tooltips.getTooltip("colorPicker");
  const onColorPickerReady = picker.once("ready");
  swatch.click();
  await onColorPickerReady;

  await simulateColorPickerChange(ruleView, picker, [0, 0, 0, 1], {
    selector: "body",
    name: "background-image",
    value: 'url("chrome://global/skin/icons/warning-64.png"), linear-gradient(rgb(0, 0, 0), rgb(255, 0, 102) 400px)'
  });

  const spectrum = picker.spectrum;
  const onHidden = picker.tooltip.once("hidden");
  const onModifications = ruleView.once("ruleview-changed");
  focusAndSendKey(spectrum.element.ownerDocument.defaultView, "RETURN");
  await onHidden;
  await onModifications;

  info("Verify again that the image preview tooltip works");
  // After a color change, the property is re-populated, we need to get the new
  // dom node
  url = getRuleViewProperty(ruleView, "body", "background").valueSpan
    .querySelector(".theme-link");
  previewTooltip = await assertShowPreviewTooltip(ruleView, url);

  await assertTooltipHiddenOnMouseOut(previewTooltip, url);
}
