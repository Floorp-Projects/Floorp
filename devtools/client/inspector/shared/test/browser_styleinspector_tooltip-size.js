/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Checking tooltips dimensions, to make sure their big enough to display their
// content

const TEST_URI = `
  <style type="text/css">
    div {
      width: 300px;height: 300px;border-radius: 50%;
      background: red url(chrome://global/skin/icons/warning-64.png);
    }
  </style>
  <div></div>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("div", inspector);
  yield testImageDimension(view);
  yield testPickerDimension(view);
});

function* testImageDimension(ruleView) {
  info("Testing background-image tooltip dimensions");

  let tooltip = ruleView.tooltips.previewTooltip;
  let panel = tooltip.panel;
  let {valueSpan} = getRuleViewProperty(ruleView, "div", "background");
  let uriSpan = valueSpan.querySelector(".theme-link");

  // Make sure there is a hover tooltip for this property, this also will fill
  // the tooltip with its content
  yield assertHoverTooltipOn(tooltip, uriSpan);

  info("Showing the tooltip");
  let onShown = tooltip.once("shown");
  tooltip.show();
  yield onShown;

  // Let's not test for a specific size, but instead let's make sure it's at
  // least as big as the image
  let imageRect = panel.querySelector("image").getBoundingClientRect();
  let panelRect = panel.getBoundingClientRect();

  ok(panelRect.width >= imageRect.width,
    "The panel is wide enough to show the image");
  ok(panelRect.height >= imageRect.height,
    "The panel is high enough to show the image");

  let onHidden = tooltip.once("hidden");
  tooltip.hide();
  yield onHidden;
}

function* testPickerDimension(ruleView) {
  info("Testing color-picker tooltip dimensions");

  let {valueSpan} = getRuleViewProperty(ruleView, "div", "background");
  let swatch = valueSpan.querySelector(".ruleview-colorswatch");
  let cPicker = ruleView.tooltips.colorPicker;

  let onShown = cPicker.tooltip.once("shown");
  swatch.click();
  yield onShown;

  // The colorpicker spectrum's iframe has a fixed width height, so let's
  // make sure the tooltip is at least as big as that
  let w = cPicker.tooltip.panel.querySelector("iframe").width;
  let h = cPicker.tooltip.panel.querySelector("iframe").height;
  let panelRect = cPicker.tooltip.panel.getBoundingClientRect();

  ok(panelRect.width >= w, "The panel is wide enough to show the picker");
  ok(panelRect.height >= h, "The panel is high enough to show the picker");

  let onHidden = cPicker.tooltip.once("hidden");
  let onRuleViewChanged = ruleView.once("ruleview-changed");
  cPicker.hide();
  yield onHidden;
  yield onRuleViewChanged;
}
