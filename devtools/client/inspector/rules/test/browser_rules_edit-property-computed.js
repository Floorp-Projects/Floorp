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

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);
  yield editAndCheck(view);
});

function* editAndCheck(view) {
  let idRuleEditor = getRuleViewRuleEditor(view, 1);
  let prop = idRuleEditor.rule.textProps[0];
  let propEditor = prop.editor;
  let newPaddingValue = "20px";

  info("Focusing the inplace editor field");
  let editor = yield focusEditableField(view, propEditor.valueSpan);
  is(inplaceEditor(propEditor.valueSpan), editor,
    "Focused editor should be the value span.");

  let onPropertyChange = waitForComputedStyleProperty("#testid", null,
    "padding-top", newPaddingValue);
  let onRefreshAfterPreview = once(view, "ruleview-changed");

  info("Entering a new value");
  EventUtils.sendString(newPaddingValue, view.styleWindow);

  info("Waiting for the throttled previewValue to apply the " +
    "changes to document");

  view.throttle.flush();
  yield onPropertyChange;

  info("Waiting for ruleview-refreshed after previewValue was applied.");
  yield onRefreshAfterPreview;

  let onBlur = once(editor.input, "blur");

  info("Entering the commit key and finishing edit");
  EventUtils.synthesizeKey("VK_RETURN", {});

  info("Waiting for blur on the field");
  yield onBlur;

  info("Waiting for the style changes to be applied");
  yield once(view, "ruleview-changed");

  let computed = prop.computed;
  let propNames = [
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
  let computedDom = propEditor.computed;
  is(computedDom.children.length, propNames.length,
    "There should be 4 nodes in the DOM");
  propNames.forEach((propName, i) => {
    is(computedDom.getElementsByClassName("ruleview-propertyvalue")[i]
      .textContent, newPaddingValue,
      "Computed value of " + propName + " in DOM is as expected");
  });
}
