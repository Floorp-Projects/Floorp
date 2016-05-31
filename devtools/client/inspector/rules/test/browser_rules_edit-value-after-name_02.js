/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that hitting shift + click on color swatch while editing the property
// name will only change the color unit and not lead to edit the property value.
// See also Bug 1248274.

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

  info("Test shift + click on color swatch while editing property name");

  yield selectNode("#testid", inspector);
  let ruleEditor = getRuleViewRuleEditor(view, 1);
  let propEditor = ruleEditor.rule.textProps[1].editor;
  let swatchSpan = propEditor.valueSpan.querySelectorAll(".ruleview-colorswatch")[2];

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

  info("wait for ruleview-changed event to be triggered to prevent pending requests");
  yield onRuleViewChanged;

  info("wait for the color unit to change");
  yield onSwatchUnitChange;
  ok(true, "the color unit was changed");

  info("wait for the property value to be updated");
  yield onPropertyValueUpdate;

  ok(!inplaceEditor(propEditor.valueSpan), "The inplace editor wasn't shown " +
    "as a result of the color swatch shift + click");
});
