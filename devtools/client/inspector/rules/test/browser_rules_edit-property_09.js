/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that a newProperty editor is only created if no other editor was
// previously displayed.

const TEST_URI = `
  <style type='text/css'>
    #testid {
      background-color: blue;
    }
  </style>
  <div id='testid'>Styled Node</div>
`;

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await selectNode("#testid", inspector);
  await testClickOnEmptyAreaToCloseEditor(inspector, view);
});

function synthesizeMouseOnEmptyArea(view) {
  // any text property editor will do
  const prop = getTextProperty(view, 1, { "background-color": "blue" });
  const propEditor = prop.editor;
  const valueContainer = propEditor.valueContainer;
  const valueRect = valueContainer.getBoundingClientRect();
  // click right next to the ";" at the end of valueContainer
  EventUtils.synthesizeMouse(
    valueContainer,
    valueRect.width + 1,
    1,
    {},
    view.styleWindow
  );
}

async function testClickOnEmptyAreaToCloseEditor(inspector, view) {
  // Start at the beginning: start to add a rule to the element's style
  // declaration, add some text, then press escape.
  const ruleEditor = getRuleViewRuleEditor(view, 1);
  const prop = getTextProperty(view, 1, { "background-color": "blue" });
  const propEditor = prop.editor;

  info("Create a property value editor");
  let editor = await focusEditableField(view, propEditor.valueSpan);
  ok(editor.input, "The inplace-editor field is ready");

  info(
    "Close the property value editor by clicking on an empty area " +
      "in the rule editor"
  );
  const onRuleViewChanged = view.once("ruleview-changed");
  let onBlur = once(editor.input, "blur");
  synthesizeMouseOnEmptyArea(view);
  await onBlur;
  await onRuleViewChanged;
  ok(!view.isEditing, "No inplace editor should be displayed in the ruleview");

  info("Create new newProperty editor by clicking again on the empty area");
  const onFocus = once(ruleEditor.element, "focus", true);
  synthesizeMouseOnEmptyArea(view);
  await onFocus;
  editor = inplaceEditor(ruleEditor.element.ownerDocument.activeElement);
  is(
    inplaceEditor(ruleEditor.newPropSpan),
    editor,
    "New property editor was created"
  );

  info("Close the newProperty editor by clicking again on the empty area");
  onBlur = once(editor.input, "blur");
  synthesizeMouseOnEmptyArea(view);
  await onBlur;

  ok(!view.isEditing, "No inplace editor should be displayed in the ruleview");
}
