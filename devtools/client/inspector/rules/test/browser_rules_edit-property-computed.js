/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the computed values of a style (the shorthand expansion) are
// properly updated after the style is changed.

const TEST_URI = `
  <style type="text/css">
    #testid {
      padding: 10px;
    }
  </style>
  <div id="testid">Styled Node</div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();
  await selectNode("#testid", inspector);
  await editAndCheck(view);
});

async function editAndCheck(view) {
  const idRuleEditor = getRuleViewRuleEditor(view, 1);
  const prop = idRuleEditor.rule.textProps[0];
  const propEditor = prop.editor;
  const newPaddingValue = "20px";

  info("Focusing the inplace editor field");
  const editor = await focusEditableField(view, propEditor.valueSpan);
  is(inplaceEditor(propEditor.valueSpan), editor,
    "Focused editor should be the value span.");

  const onPropertyChange = waitForComputedStyleProperty("#testid", null,
    "padding-top", newPaddingValue);
  const onRefreshAfterPreview = once(view, "ruleview-changed");

  info("Entering a new value");
  EventUtils.sendString(newPaddingValue, view.styleWindow);

  info("Waiting for the debounced previewValue to apply the " +
    "changes to document");

  view.debounce.flush();
  await onPropertyChange;

  info("Waiting for ruleview-refreshed after previewValue was applied.");
  await onRefreshAfterPreview;

  const onBlur = once(editor.input, "blur");

  info("Entering the commit key and finishing edit");
  EventUtils.synthesizeKey("KEY_Enter");

  info("Waiting for blur on the field");
  await onBlur;

  info("Waiting for the style changes to be applied");
  await once(view, "ruleview-changed");

  const computed = prop.computed;
  const propNames = [
    "padding-top",
    "padding-right",
    "padding-bottom",
    "padding-left"
  ];

  is(computed.length, propNames.length, "There should be 4 computed values");
  propNames.forEach((propName, i) => {
    is(computed[i].name, propName,
      "Computed property #" + i + " has name " + propName);
    is(computed[i].value, newPaddingValue,
      "Computed value of " + propName + " is as expected");
  });

  propEditor.expander.click();
  const computedDom = propEditor.computed;
  is(computedDom.children.length, propNames.length,
    "There should be 4 nodes in the DOM");
  propNames.forEach((propName, i) => {
    is(computedDom.getElementsByClassName("ruleview-propertyvalue")[i]
      .textContent, newPaddingValue,
      "Computed value of " + propName + " in DOM is as expected");
  });
}
