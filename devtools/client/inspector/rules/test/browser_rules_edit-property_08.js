/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that renaming a property works.

const TEST_URI = `
  <style type="text/css">
  #testid {
    color: #FFF;
  }
  </style>
  <div style='color: red' id='testid'>Styled Node</div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await selectNode("#testid", inspector);

  info("Get the color property editor");
  const ruleEditor = getRuleViewRuleEditor(view, 0);
  const propEditor = ruleEditor.rule.textProps[0].editor;
  is(ruleEditor.rule.textProps[0].name, "color");

  info("Focus the property name field");
  await focusEditableField(ruleEditor.ruleView, propEditor.nameSpan, 32, 1);

  info("Rename the property to background-color");
  // Expect 3 events: the value editor being focused, the ruleview-changed event
  // which signals that the new value has been previewed (fires once when the
  // value gets focused), and the markupmutation event since we're modifying an
  // inline style.
  const onValueFocus = once(ruleEditor.element, "focus", true);
  let onRuleViewChanged = ruleEditor.ruleView.once("ruleview-changed");
  const onMutation = inspector.once("markupmutation");
  EventUtils.sendString("background-color:", ruleEditor.doc.defaultView);
  await onValueFocus;
  await onRuleViewChanged;
  await onMutation;

  is(ruleEditor.rule.textProps[0].name, "background-color");
  await waitForComputedStyleProperty(
    "#testid",
    null,
    "background-color",
    "rgb(255, 0, 0)"
  );

  is(
    await getComputedStyleProperty("#testid", null, "color"),
    "rgb(255, 255, 255)",
    "color is white"
  );

  // The value field is still focused. Blur it now and wait for the
  // ruleview-changed event to avoid pending requests.
  onRuleViewChanged = view.once("ruleview-changed");
  EventUtils.synthesizeKey("KEY_Escape");
  await onRuleViewChanged;
});
