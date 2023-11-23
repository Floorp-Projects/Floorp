/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the correct editable fields are focused when tabbing and entering
// through the rule view.

const TEST_URI = `
  <style type='text/css'>
  #testid {
    background-color: blue;
    color: red;
    margin: 0;
    padding: 0;
  }
  div {
    border-color: red
  }
  </style>
  <div id='testid'>Styled Node</div>
`;

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await selectNode("#testid", inspector);

  info("Click on the selector of the inline style ('element')");
  let ruleEditor = getRuleViewRuleEditor(view, 0);
  const onFocus = once(ruleEditor.element, "focus", true);
  ruleEditor.selectorText.click();
  await onFocus;
  assertEditor(
    view,
    ruleEditor.newPropSpan,
    "Focus should be in the element property span"
  );

  info("Focus the next field with Tab");
  ruleEditor = getRuleViewRuleEditor(view, 1);
  await focusNextEditableField(view, ruleEditor);
  assertEditor(
    view,
    ruleEditor.selectorText,
    "Focus should have moved to the next rule selector"
  );

  for (let i = 0; i < ruleEditor.rule.textProps.length; i++) {
    const textProp = ruleEditor.rule.textProps[i];
    const propEditor = textProp.editor;

    info("Focus the next field with Tab");
    // Expect a ruleview-changed event if we are moving from a property value
    // to the next property name (which occurs after the first iteration, as for
    // i=0, the previous field is the selector).
    const onRuleViewChanged = i > 0 ? view.once("ruleview-changed") : null;
    await focusNextEditableField(view, ruleEditor);
    await onRuleViewChanged;
    assertEditor(
      view,
      propEditor.nameSpan,
      "Focus should have moved to the property name"
    );

    info("Focus the next field with Tab");
    await focusNextEditableField(view, ruleEditor);
    assertEditor(
      view,
      propEditor.valueSpan,
      "Focus should have moved to the property value"
    );
  }

  // Expect a ruleview-changed event again as we're bluring a property value.
  const onRuleViewChanged = view.once("ruleview-changed");
  await focusNextEditableField(view, ruleEditor);
  await onRuleViewChanged;
  assertEditor(
    view,
    ruleEditor.newPropSpan,
    "Focus should have moved to the new property span"
  );

  ruleEditor = getRuleViewRuleEditor(view, 2);

  await focusNextEditableField(view, ruleEditor);
  assertEditor(
    view,
    ruleEditor.selectorText,
    "Focus should have moved to the next rule selector"
  );

  info("Blur the selector field");
  EventUtils.synthesizeKey("KEY_Escape");
});

async function focusNextEditableField(view, ruleEditor) {
  const onFocus = once(ruleEditor.element, "focus", true);
  EventUtils.synthesizeKey("KEY_Tab", {}, view.styleWindow);
  await onFocus;
}

function assertEditor(view, element, message) {
  const editor = inplaceEditor(view.styleDocument.activeElement);
  is(inplaceEditor(element), editor, message);
}
