/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view overridden search filter does not appear for an
// unmatched rule.

const TEST_URI = `
  <style type="text/css">
    div {
      height: 0px;
    }
    #testid {
      height: 1px;
    }
    .testclass {
      height: 10px;
    }
  </style>
  <div id="testid">Styled Node</div>
  <span class="testclass">This is a span</span>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();

  await selectNode("#testid", inspector);
  await testEditSelector(view, "span");
});

async function testEditSelector(view, name) {
  info("Test editing existing selector fields");

  let ruleEditor = getRuleViewRuleEditor(view, 1);

  info("Focusing an existing selector name in the rule-view");
  const editor = await focusEditableField(view, ruleEditor.selectorText);

  is(inplaceEditor(ruleEditor.selectorText), editor,
    "The selector editor got focused");

  info("Entering a new selector name and committing");
  editor.input.value = name;

  info("Entering the commit key");
  const onRuleViewChanged = once(view, "ruleview-changed");
  EventUtils.synthesizeKey("KEY_Enter");
  await onRuleViewChanged;

  // Get the new rule editor that replaced the original
  ruleEditor = getRuleViewRuleEditor(view, 1);
  const rule = ruleEditor.rule;
  const textPropEditor = rule.textProps[0].editor;

  is(view._elementStyle.rules.length, 3, "Should have 3 rules.");
  ok(getRuleViewRule(view, name), "Rule with " + name + " selector exists.");
  ok(ruleEditor.element.getAttribute("unmatched"),
    "Rule with " + name + " does not match the current element.");
  ok(textPropEditor.filterProperty.hidden, "Overridden search is hidden.");
}
