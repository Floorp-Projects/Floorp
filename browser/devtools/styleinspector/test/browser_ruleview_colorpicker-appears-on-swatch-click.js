/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that color pickers appear when clicking on color swatches

let ruleView, swatches;

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

      testColorPickerAppearsOnColorSwatchClick();
    });
  });
}

function testColorPickerAppearsOnColorSwatchClick() {
  let cPicker = ruleView.colorPicker;
  let cPickerPanel = cPicker.tooltip.panel;
  ok(cPickerPanel, "The XUL panel for the color picker exists");

  function clickOnSwatch(index, cb) {
    if (index === swatches.length) {
      return cb();
    }

    let swatch = swatches[index];
    cPicker.tooltip.once("shown", () => {
      ok(true, "The color picker was shown on click of the color swatch");
      ok(!inplaceEditor(swatch.parentNode),
        "The inplace editor wasn't shown as a result of the color swatch click");
      cPicker.hide();
      clickOnSwatch(index + 1, cb);
    });
    swatch.click();
  }

  clickOnSwatch(0, () => {
    ruleView = swatches = null;
    finish();
  });
}
