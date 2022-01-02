/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that adding multiple values will enable the property even if the
// property does not change, and that the extra values are added correctly.

const TEST_URI = `
  <style type='text/css'>
  #testid {
    background-color: #f00;
  }
  </style>
  <div id='testid'>Styled Node</div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await selectNode("#testid", inspector);

  const rule = getRuleViewRuleEditor(view, 1).rule;
  const prop = rule.textProps[0];

  info("Disabling red background color property");
  await togglePropStatus(view, prop);
  ok(!prop.enabled, "red background-color property is disabled.");

  const editor = await focusEditableField(view, prop.editor.valueSpan);
  const onDone = view.once("ruleview-changed");
  editor.input.value = "red; color: red;";
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);
  await onDone;

  is(
    prop.editor.valueSpan.textContent,
    "red",
    "'red' property value is correctly set."
  );
  ok(prop.enabled, "red background-color property is enabled.");
  is(
    await getComputedStyleProperty("#testid", null, "background-color"),
    "rgb(255, 0, 0)",
    "red background color is set."
  );

  const propEditor = rule.textProps[1].editor;
  is(
    propEditor.nameSpan.textContent,
    "color",
    "new 'color' property name is correctly set."
  );
  is(
    propEditor.valueSpan.textContent,
    "red",
    "new 'red' property value is correctly set."
  );
  is(
    await getComputedStyleProperty("#testid", null, "color"),
    "rgb(255, 0, 0)",
    "red color is set."
  );
});
