/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the eyedropper icons in the toolbar and in the color picker aren't displayed
// when the page isn't an HTML one.

const TEST_URL = URL_ROOT + "doc_inspector_highlighter_xbl.xul";
const TEST_URL_2 =
  "data:text/html;charset=utf-8,<h1 style='color:red'>HTML test page</h1>";

add_task(function* () {
  let {inspector} = yield openInspectorForURL(TEST_URL);

  info("Check the inspector toolbar");
  let button = inspector.panelDoc.querySelector("#inspector-eyedropper-toggle");
  ok(isDisabled(button), "The button is hidden in the toolbar");

  info("Check the color picker");
  yield selectNode("#scale", inspector);

  // Find the color swatch in the rule-view.
  let ruleView = inspector.ruleview.view;
  let ruleViewDocument = ruleView.styleDocument;
  let swatchEl = ruleViewDocument.querySelector(".ruleview-colorswatch");

  info("Open the color picker");
  let cPicker = ruleView.tooltips.colorPicker;
  let onColorPickerReady = cPicker.once("ready");
  swatchEl.click();
  yield onColorPickerReady;

  button = cPicker.tooltip.doc.querySelector("#eyedropper-button");
  ok(isDisabled(button), "The button is disabled in the color picker");

  info("Navigate to a HTML document");
  yield navigateTo(inspector, TEST_URL_2);

  info("Check the inspector toolbar in HTML document");
  button = inspector.panelDoc.querySelector("#inspector-eyedropper-toggle");
  ok(!isDisabled(button), "The button is enabled in the toolbar");

  info("Check the color picker in HTML document");
  // Find the color swatch in the rule-view.
  yield selectNode("h1", inspector);

  ruleView = inspector.ruleview.view;
  ruleViewDocument = ruleView.styleDocument;
  swatchEl = ruleViewDocument.querySelector(".ruleview-colorswatch");

  info("Open the color picker in HTML document");
  cPicker = ruleView.tooltips.colorPicker;
  onColorPickerReady = cPicker.once("ready");
  swatchEl.click();
  yield onColorPickerReady;

  button = cPicker.tooltip.doc.querySelector("#eyedropper-button");
  ok(!isDisabled(button), "The button is enabled in the color picker");
});

function isDisabled(button) {
  return button.disabled;
}
