/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that reverting a selector edit does the right thing.
// Bug 1241046.

const TEST_URI = `
  <style type="text/css">
    span {
      color: chartreuse;
    }
  </style>
  <span>
    <div id="testid" class="testclass">Styled Node</div>
  </span>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();

  info("Selecting the test element");
  await selectNode("#testid", inspector);

  let idRuleEditor = getRuleViewRuleEditor(view, 2);

  info("Focusing an existing selector name in the rule-view");
  let editor = await focusEditableField(view, idRuleEditor.selectorText);

  is(inplaceEditor(idRuleEditor.selectorText), editor,
    "The selector editor got focused");

  info("Entering a new selector name and committing");
  editor.input.value = "pre";

  info("Waiting for rule view to update");
  let onRuleViewChanged = once(view, "ruleview-changed");

  info("Entering the commit key");
  EventUtils.synthesizeKey("KEY_Enter");
  await onRuleViewChanged;

  info("Re-focusing the selector name in the rule-view");
  idRuleEditor = getRuleViewRuleEditor(view, 2);
  editor = await focusEditableField(view, idRuleEditor.selectorText);

  is(view._elementStyle.rules.length, 2, "Should have 2 rules.");
  ok(getRuleViewRule(view, "pre"), "Rule with pre selector exists.");
  is(getRuleViewRuleEditor(view, 2).element.getAttribute("unmatched"),
     "true",
     "Rule with pre does not match the current element.");

  // Now change it back.
  info("Re-entering original selector name and committing");
  editor.input.value = "span";

  info("Waiting for rule view to update");
  onRuleViewChanged = once(view, "ruleview-changed");

  info("Entering the commit key");
  EventUtils.synthesizeKey("KEY_Enter");
  await onRuleViewChanged;

  is(view._elementStyle.rules.length, 2, "Should have 2 rules.");
  ok(getRuleViewRule(view, "span"), "Rule with span selector exists.");
  is(getRuleViewRuleEditor(view, 2).element.getAttribute("unmatched"),
     "false", "Rule with span matches the current element.");
});
