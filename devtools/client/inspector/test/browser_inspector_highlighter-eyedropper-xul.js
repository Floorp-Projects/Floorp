/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the eyedropper icons in the toolbar and in the color picker aren't displayed
// when the page isn't an HTML one.

const TEST_URL = URL_ROOT + "doc_inspector_eyedropper_disabled.xhtml";
const TEST_URL_2 =
  "data:text/html;charset=utf-8,<h1 style='color:red'>HTML test page</h1>";

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URL);

  info("Check the inspector toolbar");
  let button = inspector.panelDoc.querySelector("#inspector-eyedropper-toggle");
  ok(isDisabled(button), "The button is hidden in the toolbar");

  info("Check the color picker");
  await selectNode("#box", inspector);

  // Find the color swatch in the rule-view.
  let ruleView = inspector.getPanel("ruleview").view;
  let ruleViewDocument = ruleView.styleDocument;
  let swatchEl = ruleViewDocument.querySelector(".ruleview-colorswatch");

  info("Open the color picker");
  let cPicker = ruleView.tooltips.getTooltip("colorPicker");
  let onColorPickerReady = cPicker.once("ready");
  swatchEl.click();
  await onColorPickerReady;

  button = cPicker.tooltip.container.querySelector("#eyedropper-button");
  ok(isDisabled(button), "The button is disabled in the color picker");

  // Close the picker to avoid pending Promise when the connection closes because of
  // the navigation to the HTML document (See Bug 1721369).
  cPicker.hide();

  info("Navigate to a HTML document");
  const toolbarUpdated = inspector.once("inspector-toolbar-updated");
  await navigateTo(TEST_URL_2);
  await toolbarUpdated;

  info("Check the inspector toolbar in HTML document");
  button = inspector.panelDoc.querySelector("#inspector-eyedropper-toggle");
  ok(!isDisabled(button), "The button is enabled in the toolbar");

  info("Check the color picker in HTML document");
  // Find the color swatch in the rule-view.
  await selectNode("h1", inspector);

  ruleView = inspector.getPanel("ruleview").view;
  ruleViewDocument = ruleView.styleDocument;
  swatchEl = ruleViewDocument.querySelector(".ruleview-colorswatch");

  info("Open the color picker in HTML document");
  cPicker = ruleView.tooltips.getTooltip("colorPicker");
  onColorPickerReady = cPicker.once("ready");
  swatchEl.click();
  await onColorPickerReady;

  button = cPicker.tooltip.container.querySelector("#eyedropper-button");
  ok(!isDisabled(button), "The button is enabled in the color picker");
});

function isDisabled(button) {
  return button.disabled;
}
