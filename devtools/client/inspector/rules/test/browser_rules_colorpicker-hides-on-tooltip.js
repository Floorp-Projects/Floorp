/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the color picker tooltip hides when an image tooltip appears.

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

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {view} = yield openRuleView();

  let swatch = getRuleViewProperty(view, "body", "color").valueSpan
    .querySelector(".ruleview-colorswatch");

  let bgImageSpan = getRuleViewProperty(view, "body", "background-image").valueSpan;
  let uriSpan = bgImageSpan.querySelector(".theme-link");

  let colorPicker = view.tooltips.colorPicker;
  info("Showing the color picker tooltip by clicking on the color swatch");
  let onColorPickerReady = colorPicker.once("ready");
  swatch.click();
  yield onColorPickerReady;

  info("Now showing the image preview tooltip to hide the color picker");
  let onHidden = colorPicker.tooltip.once("hidden");
  // Hiding the color picker refreshes the value.
  let onRuleViewChanged = view.once("ruleview-changed");
  yield assertHoverTooltipOn(view.tooltips.previewTooltip, uriSpan);
  yield onHidden;
  yield onRuleViewChanged;

  ok(true, "The color picker closed when the image preview tooltip appeared");
});
