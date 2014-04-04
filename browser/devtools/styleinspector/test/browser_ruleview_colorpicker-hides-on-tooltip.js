/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the color picker tooltip hides when an image or transform
// tooltip appears

let ruleView, swatches;

const PAGE_CONTENT = [
  '<style type="text/css">',
  '  body {',
  '    color: red;',
  '    background-color: #ededed;',
  '    background-image: url(chrome://global/skin/icons/warning-64.png);',
  '    border: 2em solid rgba(120, 120, 120, .5);',
  '    transform: skew(-16deg);',
  '  }',
  '</style>',
  'Testing the color picker tooltip!'
].join("\n");

function test() {
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function load(evt) {
    gBrowser.selectedBrowser.removeEventListener("load", load, true);
    waitForFocus(createDocument, content);
  }, true);

  content.location = "data:text/html,rule view color picker tooltip test";
}

function createDocument() {
  content.document.body.innerHTML = PAGE_CONTENT;

  openRuleView((inspector, view) => {
    ruleView = view;
    inspector.once("inspector-updated", () => {
      let cSwatch = getRuleViewProperty("color", ruleView).valueSpan
        .querySelector(".ruleview-colorswatch");
      let bgSwatch = getRuleViewProperty("background-color", ruleView).valueSpan
        .querySelector(".ruleview-colorswatch");
      let bSwatch = getRuleViewProperty("border", ruleView).valueSpan
        .querySelector(".ruleview-colorswatch");
      swatches = [cSwatch, bgSwatch, bSwatch];

      testColorPickerHidesWhenImageTooltipAppears();
    });
  });
}

function testColorPickerHidesWhenImageTooltipAppears() {
  Task.spawn(function*() {
    let swatch = swatches[0];
    let bgImageSpan = getRuleViewProperty("background-image", ruleView).valueSpan;
    let uriSpan = bgImageSpan.querySelector(".theme-link");
    let tooltip = ruleView.colorPicker.tooltip;

    info("Showing the color picker tooltip by clicking on the color swatch");
    let onShown = tooltip.once("shown");
    swatch.click();
    yield onShown;

    info("Now showing the image preview tooltip to hide the color picker");
    let onHidden = tooltip.once("hidden");
    yield assertTooltipShownOn(ruleView.previewTooltip, uriSpan);
    yield onHidden;

    ok(true, "The color picker closed when the image preview tooltip appeared");
  }).then(testColorPickerHidesWhenTransformTooltipAppears);
}

function testColorPickerHidesWhenTransformTooltipAppears() {
  Task.spawn(function*() {
    let swatch = swatches[0];
    let transformSpan = getRuleViewProperty("transform", ruleView).valueSpan;
    let tooltip = ruleView.colorPicker.tooltip;

    info("Showing the color picker tooltip by clicking on the color swatch");
    let onShown = tooltip.once("shown");
    swatch.click();
    yield onShown;

    info("Now showing the transform preview tooltip to hide the color picker");
    let onHidden = tooltip.once("hidden");
    yield assertTooltipShownOn(ruleView.previewTooltip, transformSpan);
    yield onHidden;

    ok(true, "The color picker closed when the transform preview tooltip appeared");
  }).then(() => {
    swatches = ruleView = null;
    finish();
  });
}
