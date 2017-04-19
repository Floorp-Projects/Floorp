/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
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
      background: red url("chrome://global/skin/icons/warning-64.png")
        no-repeat center center;
    }
  </style>
  Testing the color picker tooltip!
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {view} = yield openRuleView();
  yield testColorChangeIsntRevertedWhenOtherTooltipIsShown(view);
});

function* testColorChangeIsntRevertedWhenOtherTooltipIsShown(ruleView) {
  let swatch = getRuleViewProperty(ruleView, "body", "background").valueSpan
    .querySelector(".ruleview-colorswatch");

  info("Open the color picker tooltip and change the color");
  let picker = ruleView.tooltips.getTooltip("colorPicker");
  let onColorPickerReady = picker.once("ready");
  swatch.click();
  yield onColorPickerReady;

  yield simulateColorPickerChange(ruleView, picker, [0, 0, 0, 1], {
    selector: "body",
    name: "background-color",
    value: "rgb(0, 0, 0)"
  });

  let spectrum = picker.spectrum;

  let onModifications = waitForNEvents(ruleView, "ruleview-changed", 2);
  let onHidden = picker.tooltip.once("hidden");
  focusAndSendKey(spectrum.element.ownerDocument.defaultView, "RETURN");
  yield onHidden;
  yield onModifications;

  info("Open the image preview tooltip");
  let value = getRuleViewProperty(ruleView, "body", "background").valueSpan;
  let url = value.querySelector(".theme-link");
  let previewTooltip = yield assertShowPreviewTooltip(ruleView, url);

  info("Image tooltip is shown, verify that the swatch is still correct");
  swatch = value.querySelector(".ruleview-colorswatch");
  is(swatch.style.backgroundColor, "black",
    "The swatch's color is correct");
  is(swatch.nextSibling.textContent, "black", "The color name is correct");

  yield assertTooltipHiddenOnMouseOut(previewTooltip, url);
}
