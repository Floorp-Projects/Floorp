/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that after a color change, opening another tooltip, like the image
// preview doesn't revert the color change in the ruleView.
// This used to happen when the activeSwatch wasn't reset when the colorpicker
// would hide.
// See bug 979292

const PAGE_CONTENT = [
  '<style type="text/css">',
  '  body {',
  '    background: red url("chrome://global/skin/icons/warning-64.png") no-repeat center center;',
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

  openRuleView((inspector, ruleView) => {
    inspector.once("inspector-updated", () => {
      testColorChangeIsntRevertedWhenOtherTooltipIsShown(ruleView);
    });
  });
}

function testColorChangeIsntRevertedWhenOtherTooltipIsShown(ruleView) {
  Task.spawn(function*() {
    let swatch = getRuleViewProperty("background", ruleView).valueSpan
      .querySelector(".ruleview-colorswatch");

    info("Open the color picker tooltip and change the color");
    let picker = ruleView.colorPicker;
    let onShown = picker.tooltip.once("shown");
    swatch.click();
    yield onShown;

    yield simulateColorChange(picker, [0, 0, 0, 1], {
      element: content.document.body,
      name: "backgroundColor",
      value: "rgb(0, 0, 0)"
    });
    let spectrum = yield picker.spectrum;
    let onHidden = picker.tooltip.once("hidden");
    EventUtils.sendKey("RETURN", spectrum.element.ownerDocument.defaultView);
    yield onHidden;

    info("Open the image preview tooltip");
    let value = getRuleViewProperty("background", ruleView).valueSpan;
    let url = value.querySelector(".theme-link");
    let onShown = ruleView.previewTooltip.once("shown");
    let anchor = yield isHoverTooltipTarget(ruleView.previewTooltip, url);
    ruleView.previewTooltip.show(anchor);
    yield onShown;

    info("Image tooltip is shown, verify that the swatch is still correct");
    let swatch = value.querySelector(".ruleview-colorswatch");
    is(swatch.style.backgroundColor, "rgb(0, 0, 0)", "The swatch's color is correct");
    is(swatch.nextSibling.textContent, "#000", "The color name is correct");
  }).then(finish);
}
