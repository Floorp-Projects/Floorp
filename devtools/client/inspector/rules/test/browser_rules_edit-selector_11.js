/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Regression test for bug 1293616, where editing a selector should
// change the relative priority of the rule.

const TEST_URI = `
  <style type="text/css">
    #testid {
      background: aqua;
    }
    .pickme {
      background: seagreen;
    }
    span {
      background: fuchsia;
    }
  </style>
  <div>
    <span id="testid" class="pickme">
      Styled Node
    </span>
  </div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();
  await selectNode(".pickme", inspector);
  await testEditSelector(view);
});

async function testEditSelector(view) {
  let ruleEditor = getRuleViewRuleEditor(view, 1);
  const editor = await focusEditableField(view, ruleEditor.selectorText);

  editor.input.value = ".pickme";
  const onRuleViewChanged = once(view, "ruleview-changed");
  EventUtils.synthesizeKey("KEY_Enter");
  await onRuleViewChanged;

  // Escape the new property editor after editing the selector
  const onBlur = once(view.styleDocument.activeElement, "blur");
  EventUtils.synthesizeKey("KEY_Escape", {}, view.styleWindow);
  await onBlur;

  // Get the new rule editor that replaced the original
  ruleEditor = getRuleViewRuleEditor(view, 1);

  info("Check that the correct rules are visible");
  is(view._elementStyle.rules.length, 4, "Should have 4 rules.");
  is(ruleEditor.element.getAttribute("unmatched"), "false", "Rule editor is matched.");

  let props = ruleEditor.rule.textProps;
  is(props.length, 1, "Rule has correct number of properties");
  is(props[0].name, "background", "Found background property");
  is(props[0].value, "aqua", "Background property is aqua");
  ok(props[0].overridden, "Background property is overridden");

  ruleEditor = getRuleViewRuleEditor(view, 2);
  props = ruleEditor.rule.textProps;
  is(props.length, 1, "Rule has correct number of properties");
  is(props[0].name, "background", "Found background property");
  is(props[0].value, "seagreen", "Background property is seagreen");
  ok(!props[0].overridden, "Background property is not overridden");
}
