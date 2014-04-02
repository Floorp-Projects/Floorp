/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that changing a color in a gradient css declaration using the tooltip
// color picker works

let {gDevTools} = Cu.import("resource:///modules/devtools/gDevTools.jsm", {});

let contentDoc;
let contentWin;
let inspector;
let ruleView;

const PAGE_CONTENT = [
  '<style type="text/css">',
  '  body {',
  '    background-image: linear-gradient(to left, #f06 25%, #333 95%, #000 100%);',
  '  }',
  '</style>',
  'Updating a gradient declaration with the color picker tooltip'
].join("\n");

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, arguments.callee, true);
    contentDoc = content.document;
    contentWin = contentDoc.defaultView;
    waitForFocus(createDocument, content);
  }, true);

  content.location = "data:text/html,rule view color picker tooltip test";
}

function createDocument() {
  contentDoc.body.innerHTML = PAGE_CONTENT;

  openRuleView((aInspector, aRuleView) => {
    inspector = aInspector;
    ruleView = aRuleView;
    startTests();
  });
}

function startTests() {
  inspector.selection.setNode(contentDoc.body);
  inspector.once("inspector-updated", testColorParsing);
}

function endTests() {
  executeSoon(function() {
    gDevTools.once("toolbox-destroyed", () => {
      contentDoc = contentWin = inspector = ruleView = null;
      gBrowser.removeCurrentTab();
      finish();
    });
    inspector._toolbox.destroy();
  });
}

function testColorParsing() {
  let ruleEl = getRuleViewProperty("background-image");
  ok(ruleEl, "The background-image gradient declaration was found");

  let swatchEls = ruleEl.valueSpan.querySelectorAll(".ruleview-colorswatch");
  ok(swatchEls, "The color swatch elements were found");
  is(swatchEls.length, 3, "There are 3 color swatches");

  let colorEls = ruleEl.valueSpan.querySelectorAll(".ruleview-color");
  ok(colorEls, "The color elements were found");
  is(colorEls.length, 3, "There are 3 color values");

  let colors = ["#F06", "#333", "#000"];
  for (let i = 0; i < colors.length; i ++) {
    is(colorEls[i].textContent, colors[i], "The right color value was found");
  }

  testPickingNewColor();
}

function testPickingNewColor() {
  // Grab the first color swatch and color in the gradient
  let ruleEl = getRuleViewProperty("background-image");
  let swatchEl = ruleEl.valueSpan.querySelector(".ruleview-colorswatch");
  let colorEl = ruleEl.valueSpan.querySelector(".ruleview-color");

  // Get the color picker tooltip
  let cPicker = ruleView.colorPicker;

  cPicker.tooltip.once("shown", () => {
    simulateColorChange(cPicker, [1, 1, 1, 1]);

    executeSoon(() => {
      is(swatchEl.style.backgroundColor, "rgb(1, 1, 1)",
        "The color swatch's background was updated");
      is(colorEl.textContent, "rgba(1, 1, 1, 1)",
        "The color text was updated");
      is(content.getComputedStyle(content.document.body).backgroundImage,
        "linear-gradient(to left, rgb(255, 0, 102) 25%, rgb(51, 51, 51) 95%, rgb(0, 0, 0) 100%)",
        "The gradient has been updated correctly");

      cPicker.hide();
      endTests();
    });
  });
  swatchEl.click();
}

function simulateColorChange(colorPicker, newRgba) {
  // Note that this test isn't concerned with simulating events to test how the
  // spectrum color picker reacts, see browser_spectrum.js for this.
  // This test only cares about the color swatch <-> color picker <-> rule view
  // interactions. That's why there's no event simulations here
  colorPicker.spectrum.then(spectrum => {
    spectrum.rgb = newRgba;
    spectrum.updateUI();
    spectrum.onChange();
  });
}

function getRuleViewProperty(name) {
  let prop = null;
  [].forEach.call(ruleView.doc.querySelectorAll(".ruleview-property"), property => {
    let nameSpan = property.querySelector(".ruleview-propertyname");
    let valueSpan = property.querySelector(".ruleview-propertyvalue");

    if (nameSpan.textContent === name) {
      prop = {nameSpan: nameSpan, valueSpan: valueSpan};
    }
  });
  return prop;
}
