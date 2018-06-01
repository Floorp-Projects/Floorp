/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that clicking on swatch-preceeded value while editing the property name
// will result in editing the property value. Also tests that the value span is updated
// only if the property name has changed. See also Bug 1248274.

const TEST_URI = `
  <style type="text/css">
  #testid {
    color: red;
  }
  </style>
  <div id="testid">Styled Node</div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();

  await selectNode("#testid", inspector);

  const ruleEditor = getRuleViewRuleEditor(view, 1);
  const propEditor = ruleEditor.rule.textProps[0].editor;

  await testColorValueSpanClickWithoutNameChange(propEditor, view);
  await testColorValueSpanClickAfterNameChange(propEditor, view);
});

async function testColorValueSpanClickWithoutNameChange(propEditor, view) {
  info("Test click on color span while focusing property name editor");
  const colorSpan = propEditor.valueSpan.querySelector(".ruleview-color");

  info("Focus the color name span");
  await focusEditableField(view, propEditor.nameSpan);
  let editor = inplaceEditor(propEditor.doc.activeElement);

  // We add a click event to make sure the color span won't be cleared
  // on nameSpan blur (which would lead to the click event not being triggered)
  const onColorSpanClick = once(colorSpan, "click");

  // The property-value-updated is emitted when the valueSpan markup is being
  // re-populated, which should not be the case when not modifying the property name
  const onPropertyValueUpdated = function() {
    ok(false, "The \"property-value-updated\" should not be emitted");
  };
  view.on("property-value-updated", onPropertyValueUpdated);

  info("blur propEditor.nameSpan by clicking on the color span");
  EventUtils.synthesizeMouse(colorSpan, 1, 1, {}, propEditor.doc.defaultView);

  info("wait for the click event on the color span");
  await onColorSpanClick;
  ok(true, "Expected click event was emitted");

  editor = inplaceEditor(propEditor.doc.activeElement);
  is(inplaceEditor(propEditor.valueSpan), editor,
    "The property value editor got focused");

  // We remove this listener in order to not cause unwanted conflict in the next test
  view.off("property-value-updated", onPropertyValueUpdated);

  info("blur valueSpan editor to trigger ruleview-changed event and prevent " +
    "having pending request");
  const onRuleViewChanged = view.once("ruleview-changed");
  editor.input.blur();
  await onRuleViewChanged;
}

async function testColorValueSpanClickAfterNameChange(propEditor, view) {
  info("Test click on color span after property name change");
  const colorSpan = propEditor.valueSpan.querySelector(".ruleview-color");

  info("Focus the color name span");
  await focusEditableField(view, propEditor.nameSpan);
  let editor = inplaceEditor(propEditor.doc.activeElement);

  info("Modify the property to border-color to trigger the " +
    "property-value-updated event");
  editor.input.value = "border-color";

  let onRuleViewChanged = view.once("ruleview-changed");
  const onPropertyValueUpdate = view.once("property-value-updated");

  info("blur propEditor.nameSpan by clicking on the color span");
  EventUtils.synthesizeMouse(colorSpan, 1, 1, {}, propEditor.doc.defaultView);

  info("wait for ruleview-changed event to be triggered to prevent pending requests");
  await onRuleViewChanged;

  info("wait for the property value to be updated");
  await onPropertyValueUpdate;
  ok(true, "Expected \"property-value-updated\" event was emitted");

  editor = inplaceEditor(propEditor.doc.activeElement);
  is(inplaceEditor(propEditor.valueSpan), editor,
    "The property value editor got focused");

  info("blur valueSpan editor to trigger ruleview-changed event and prevent " +
    "having pending request");
  onRuleViewChanged = view.once("ruleview-changed");
  editor.input.blur();
  await onRuleViewChanged;
}
