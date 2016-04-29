/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that clicking on swatch-preceeded value while editing the property name
// will result in editing the property value. See also Bug 1248274.

const TEST_URI = `
  <style type="text/css">
  #testid {
    color: red;
    background: linear-gradient(
      90deg,
      rgb(183,222,237),
      rgb(33,180,226),
      rgb(31,170,217),
      rgba(200,170,140,0.5));
  }
  </style>
  <div id="testid">Styled Node</div>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();

  yield selectNode("#testid", inspector);

  let ruleEditor = getRuleViewRuleEditor(view, 1);
  yield testColorValueSpanClick(ruleEditor, view);
  yield testColorSwatchShiftClick(ruleEditor, view);
  yield testColorSwatchClick(ruleEditor, view);
});

function* testColorValueSpanClick(ruleEditor, view) {
  info("Test click on color span while editing property name");
  let propEditor = ruleEditor.rule.textProps[0].editor;
  let colorSpan = propEditor.valueSpan.querySelector(".ruleview-color");

  info("Focus the color name span");
  yield focusEditableField(view, propEditor.nameSpan);
  let editor = inplaceEditor(propEditor.doc.activeElement);

  info("Modify the property to border-color to trigger the " +
    "property-value-updated event");
  editor.input.value = "border-color";

  let onPropertyValueUpdate = view.once("property-value-updated");

  info("blur propEditor.nameSpan by clicking on the color span");
  EventUtils.synthesizeMouse(colorSpan, 1, 1, {}, propEditor.doc.defaultView);

  info("wait for the property value to be updated");
  yield onPropertyValueUpdate;

  editor = inplaceEditor(propEditor.doc.activeElement);
  is(inplaceEditor(propEditor.valueSpan), editor,
    "The property value editor got focused");

  info("blur valueSpan editor to trigger ruleview-changed event and prevent " +
    "having pending request");
  let onRuleViewChanged = view.once("ruleview-changed");
  editor.input.blur();
  yield onRuleViewChanged;
}

function* testColorSwatchShiftClick(ruleEditor, view) {
  info("Test shift + click on color swatch while editing property name");
  let propEditor = ruleEditor.rule.textProps[1].editor;
  let swatchSpan = propEditor.valueSpan.querySelectorAll(
    ".ruleview-colorswatch")[2];

  info("Focus the background name span");
  yield focusEditableField(view, propEditor.nameSpan);
  let editor = inplaceEditor(propEditor.doc.activeElement);

  info("Modify the property to background-image to trigger the " +
    "property-value-updated event");
  editor.input.value = "background-image";

  let onPropertyValueUpdate = view.once("property-value-updated");
  let onSwatchUnitChange = swatchSpan.once("unit-change");
  let onRuleViewChanged = view.once("ruleview-changed");

  info("blur propEditor.nameSpan by clicking on the color swatch");
  EventUtils.synthesizeMouseAtCenter(swatchSpan, {shiftKey: true},
    propEditor.doc.defaultView);

  info("wait for ruleview-changed event to be triggered to prevent pending " +
    "requests");
  yield onRuleViewChanged;

  info("wait for the color unit to change");
  yield onSwatchUnitChange;
  ok(true, "the color unit was changed");

  info("wait for the property value to be updated");
  yield onPropertyValueUpdate;

  ok(!inplaceEditor(propEditor.valueSpan), "The inplace editor wasn't shown " +
    "as a result of the color swatch shift + click");
}

function* testColorSwatchClick(ruleEditor, view) {
  info("Test click on color swatch while editing property name");
  let propEditor = ruleEditor.rule.textProps[1].editor;
  let swatchSpan = propEditor.valueSpan.querySelectorAll(
    ".ruleview-colorswatch")[3];
  let colorPicker = view.tooltips.colorPicker;

  info("Focus the background-image name span");
  yield focusEditableField(view, propEditor.nameSpan);
  let editor = inplaceEditor(propEditor.doc.activeElement);

  info("Modify the property to background to trigger the " +
    "property-value-updated event");
  editor.input.value = "background";

  let onRuleViewChanged = view.once("ruleview-changed");
  let onPropertyValueUpdate = view.once("property-value-updated");
  let onShown = colorPicker.tooltip.once("shown");

  info("blur propEditor.nameSpan by clicking on the color swatch");
  EventUtils.synthesizeMouseAtCenter(swatchSpan, {},
    propEditor.doc.defaultView);

  info("wait for ruleview-changed event to be triggered to prevent pending " +
    "requests");
  yield onRuleViewChanged;

  info("wait for the property value to be updated");
  yield onPropertyValueUpdate;

  info("wait for the color picker to be shown");
  yield onShown;

  ok(true, "The color picker was shown on click of the color swatch");
  ok(!inplaceEditor(propEditor.valueSpan),
    "The inplace editor wasn't shown as a result of the color swatch click");

  let spectrum = yield colorPicker.spectrum;
  is(spectrum.rgb, "200,170,140,0.5", "The correct color picker was shown");
}
