/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the color picker tooltip hides when an image tooltip appears

const PAGE_CONTENT = [
  '<style type="text/css">',
  '  body {',
  '    color: red;',
  '    background-color: #ededed;',
  '    background-image: url(chrome://global/skin/icons/warning-64.png);',
  '    border: 2em solid rgba(120, 120, 120, .5);',
  '  }',
  '</style>',
  'Testing the color picker tooltip!'
].join("\n");

let test = asyncTest(function*() {
  yield addTab("data:text/html,rule view color picker tooltip test");
  content.document.body.innerHTML = PAGE_CONTENT;
  let {toolbox, inspector, view} = yield openRuleView();

  let swatch = getRuleViewProperty(view, "body", "color").valueSpan
    .querySelector(".ruleview-colorswatch");

  yield testColorPickerHidesWhenImageTooltipAppears(view, swatch);
});

function* testColorPickerHidesWhenImageTooltipAppears(view, swatch) {
  let bgImageSpan = getRuleViewProperty(view, "body", "background-image").valueSpan;
  let uriSpan = bgImageSpan.querySelector(".theme-link");
  let tooltip = view.tooltips.colorPicker.tooltip;

  info("Showing the color picker tooltip by clicking on the color swatch");
  let onShown = tooltip.once("shown");
  swatch.click();
  yield onShown;

  info("Now showing the image preview tooltip to hide the color picker");
  let onHidden = tooltip.once("hidden");
  yield assertHoverTooltipOn(view.tooltips.previewTooltip, uriSpan);
  yield onHidden;

  ok(true, "The color picker closed when the image preview tooltip appeared");
}
