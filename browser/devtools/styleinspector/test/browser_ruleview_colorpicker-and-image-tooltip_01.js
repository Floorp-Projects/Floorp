/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that after a color change, the image preview tooltip in the same
// property is displayed and positioned correctly.
// See bug 979292

const PAGE_CONTENT = [
  '<style type="text/css">',
  '  body {',
  '    background: url("chrome://global/skin/icons/warning-64.png"), linear-gradient(white, #F06 400px);',
  '  }',
  '</style>',
  'Testing the color picker tooltip!'
].join("\n");

let test = asyncTest(function*() {
  yield addTab("data:text/html,rule view color picker tooltip test");
  content.document.body.innerHTML = PAGE_CONTENT;
  let {toolbox, inspector, view} = yield openRuleView();

  let value = getRuleViewProperty(view, "body", "background").valueSpan;
  let swatch = value.querySelector(".ruleview-colorswatch");
  let url = value.querySelector(".theme-link");
  yield testImageTooltipAfterColorChange(swatch, url, view);
});

function* testImageTooltipAfterColorChange(swatch, url, ruleView) {
  info("First, verify that the image preview tooltip works");
  let anchor = yield isHoverTooltipTarget(ruleView.previewTooltip, url);
  ok(anchor, "The image preview tooltip is shown on the url span");
  is(anchor, url, "The anchor returned by the showOnHover callback is correct");

  info("Open the color picker tooltip and change the color");
  let picker = ruleView.colorPicker;
  let onShown = picker.tooltip.once("shown");
  swatch.click();
  yield onShown;
  yield simulateColorPickerChange(picker, [0, 0, 0, 1], {
    element: content.document.body,
    name: "backgroundImage",
    value: 'url("chrome://global/skin/icons/warning-64.png"), linear-gradient(rgb(0, 0, 0), rgb(255, 0, 102) 400px)'
  });

  let spectrum = yield picker.spectrum;
  let onHidden = picker.tooltip.once("hidden");
  EventUtils.sendKey("RETURN", spectrum.element.ownerDocument.defaultView);
  yield onHidden;

  info("Verify again that the image preview tooltip works");
  // After a color change, the property is re-populated, we need to get the new
  // dom node
  url = getRuleViewProperty(ruleView, "body", "background").valueSpan.querySelector(".theme-link");
  let anchor = yield isHoverTooltipTarget(ruleView.previewTooltip, url);
  ok(anchor, "The image preview tooltip is shown on the url span");
  is(anchor, url, "The anchor returned by the showOnHover callback is correct");
}
