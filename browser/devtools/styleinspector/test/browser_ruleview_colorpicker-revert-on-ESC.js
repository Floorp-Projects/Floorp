/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that a color change in the color picker is reverted when ESC is
// pressed.

const TEST_URI = `
  <style type="text/css">
    body {
      background-color: #EDEDED;
    }
  </style>
`;

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {view} = yield openRuleView();
  yield testPressingEscapeRevertsChanges(view);
  yield testPressingEscapeRevertsChangesAndDisables(view);
});

function* testPressingEscapeRevertsChanges(view) {
  let ruleEditor = getRuleViewRuleEditor(view, 1);
  let propEditor = ruleEditor.rule.textProps[0].editor;
  let swatch = propEditor.valueSpan.querySelector(".ruleview-colorswatch");
  let cPicker = view.tooltips.colorPicker;

  let onShown = cPicker.tooltip.once("shown");
  swatch.click();
  yield onShown;

  yield simulateColorPickerChange(view, cPicker, [0, 0, 0, 1], {
    element: content.document.body,
    name: "backgroundColor",
    value: "rgb(0, 0, 0)"
  });

  is(swatch.style.backgroundColor, "rgb(0, 0, 0)",
    "The color swatch's background was updated");
  is(propEditor.valueSpan.textContent, "#000",
    "The text of the background-color css property was updated");

  let spectrum = yield cPicker.spectrum;

  info("Pressing ESCAPE to close the tooltip");
  let onHidden = cPicker.tooltip.once("hidden");
  EventUtils.sendKey("ESCAPE", spectrum.element.ownerDocument.defaultView);
  yield onHidden;
  yield ruleEditor.rule._applyingModifications;

  yield waitForComputedStyleProperty("body", null, "background-color",
    "rgb(237, 237, 237)");
  is(propEditor.valueSpan.textContent, "#EDEDED",
    "Got expected property value.");
}

function* testPressingEscapeRevertsChangesAndDisables(view) {
  let ruleEditor = getRuleViewRuleEditor(view, 1);
  let propEditor = ruleEditor.rule.textProps[0].editor;
  let swatch = propEditor.valueSpan.querySelector(".ruleview-colorswatch");
  let cPicker = view.tooltips.colorPicker;

  info("Disabling background-color property");
  propEditor.enable.click();
  yield ruleEditor.rule._applyingModifications;

  ok(propEditor.element.classList.contains("ruleview-overridden"),
    "property is overridden.");
  is(propEditor.enable.style.visibility, "visible",
    "property enable checkbox is visible.");
  ok(!propEditor.enable.getAttribute("checked"),
    "property enable checkbox is not checked.");
  ok(!propEditor.prop.enabled,
    "background-color property is disabled.");
  let newValue = yield getRulePropertyValue("background-color");
  is(newValue, "", "background-color should have been unset.");

  let onShown = cPicker.tooltip.once("shown");
  swatch.click();
  yield onShown;

  ok(!propEditor.element.classList.contains("ruleview-overridden"),
    "property overridden is not displayed.");
  is(propEditor.enable.style.visibility, "hidden",
    "property enable checkbox is hidden.");

  let spectrum = yield cPicker.spectrum;
  info("Simulating a color picker change in the widget");
  spectrum.rgb = [0, 0, 0, 1];
  yield ruleEditor.rule._applyingModifications;

  info("Pressing ESCAPE to close the tooltip");
  let onHidden = cPicker.tooltip.once("hidden");
  EventUtils.sendKey("ESCAPE", spectrum.element.ownerDocument.defaultView);
  yield onHidden;
  yield ruleEditor.rule._applyingModifications;

  ok(propEditor.element.classList.contains("ruleview-overridden"),
    "property is overridden.");
  is(propEditor.enable.style.visibility, "visible",
    "property enable checkbox is visible.");
  ok(!propEditor.enable.getAttribute("checked"),
    "property enable checkbox is not checked.");
  ok(!propEditor.prop.enabled,
    "background-color property is disabled.");
  newValue = yield getRulePropertyValue("background-color");
  is(newValue, "", "background-color should have been unset.");
  is(propEditor.valueSpan.textContent, "#EDEDED",
    "Got expected property value.");
}

function* getRulePropertyValue(name) {
  let propValue = yield executeInContent("Test:GetRulePropertyValue", {
    styleSheetIndex: 0,
    ruleIndex: 0,
    name: name
  });
  return propValue;
}
