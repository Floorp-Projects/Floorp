/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that color pickers appear when clicking on color swatches

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

  let cSwatch = getRuleViewProperty(view, "body", "color").valueSpan
    .querySelector(".ruleview-colorswatch");
  let bgSwatch = getRuleViewProperty(view, "body", "background-color").valueSpan
    .querySelector(".ruleview-colorswatch");
  let bSwatch = getRuleViewProperty(view, "body", "border").valueSpan
    .querySelector(".ruleview-colorswatch");

  for (let swatch of [cSwatch, bgSwatch, bSwatch]) {
    info("Testing that the colorpicker appears colorswatch click");
    yield testColorPickerAppearsOnColorSwatchClick(view, swatch);
  }
});

function* testColorPickerAppearsOnColorSwatchClick(view, swatch) {
  let cPicker = view.colorPicker;
  ok(cPicker, "The rule-view has the expected colorPicker property");

  let cPickerPanel = cPicker.tooltip.panel;
  ok(cPickerPanel, "The XUL panel for the color picker exists");

  let onShown = cPicker.tooltip.once("shown");
  swatch.click();
  yield onShown;

  ok(true, "The color picker was shown on click of the color swatch");
  ok(!inplaceEditor(swatch.parentNode),
    "The inplace editor wasn't shown as a result of the color swatch click");
  cPicker.hide();
}
