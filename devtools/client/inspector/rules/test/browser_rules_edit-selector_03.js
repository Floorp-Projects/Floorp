/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Testing selector inplace-editor behaviors in the rule-view with invalid
// selectors

const TEST_URI = `
  <style type="text/css">
    .testclass {
      text-align: center;
    }
  </style>
  <div class="testclass">Styled Node</div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await selectNode(".testclass", inspector);
  await testEditSelector(view, "asd@:::!");
});

async function testEditSelector(view, name) {
  info("Test editing existing selector fields");

  const ruleEditor = getRuleViewRuleEditor(view, 1);

  info("Focusing an existing selector name in the rule-view");
  const editor = await focusEditableField(view, ruleEditor.selectorText);

  is(
    inplaceEditor(ruleEditor.selectorText),
    editor,
    "The selector editor got focused"
  );

  info("Entering a new selector name and committing");
  editor.input.value = name;
  const onRuleViewChanged = once(view, "ruleview-invalid-selector");
  EventUtils.synthesizeKey("KEY_Enter");
  await onRuleViewChanged;

  is(view._elementStyle.rules.length, 2, "Should have 2 rules.");
  is(
    getRuleViewRule(view, name),
    undefined,
    "Rule with " + name + " selector should not exist."
  );
  ok(
    getRuleViewRule(view, ".testclass"),
    "Rule with .testclass selector exists."
  );
}
