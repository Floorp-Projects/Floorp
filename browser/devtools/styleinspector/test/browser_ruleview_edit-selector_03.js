/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Testing selector inplace-editor behaviors in the rule-view with invalid
// selectors

let TEST_URI = [
  '<style type="text/css">',
  '  .testclass {',
  '    text-align: center;',
  '  }',
  '</style>',
  '<div class="testclass">Styled Node</div>',
].join("\n");

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode(".testclass", inspector);
  yield testEditSelector(view, "asd@:::!");
});

function* testEditSelector(view, name) {
  info("Test editing existing selector fields");

  let ruleEditor = getRuleViewRuleEditor(view, 1);

  info("Focusing an existing selector name in the rule-view");
  let editor = yield focusEditableField(ruleEditor.selectorText);

  is(inplaceEditor(ruleEditor.selectorText), editor,
    "The selector editor got focused");

  info("Entering a new selector name and committing");
  editor.input.value = name;
  EventUtils.synthesizeKey("VK_RETURN", {});

  is(view._elementStyle.rules.length, 2, "Should have 2 rules.");
  is(getRuleViewRule(view, name), undefined,
    "Rule with " + name + " selector should not exist.");
  ok(getRuleViewRule(view, ".testclass"),
    "Rule with .testclass selector exists.");
}
