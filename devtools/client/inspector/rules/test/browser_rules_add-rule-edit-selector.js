/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the behaviour of adding a new rule to the rule view and editing
// its selector.

const TEST_URI = `
  <style type="text/css">
    #testid {
      text-align: center;
    }
  </style>
  <div id="testid">Styled Node</div>
  <span>This is a span</span>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();
  await selectNode("#testid", inspector);

  await addNewRule(inspector, view);
  await testEditSelector(view, "span");

  info("Selecting the modified element with the new rule");
  await selectNode("span", inspector);
  await checkModifiedElement(view, "span");
});

async function testEditSelector(view, name) {
  info("Test editing existing selector field");
  const idRuleEditor = getRuleViewRuleEditor(view, 1);
  const editor = idRuleEditor.selectorText.ownerDocument.activeElement;

  info("Entering a new selector name and committing");
  editor.value = name;

  info("Waiting for rule view to update");
  const onRuleViewChanged = once(view, "ruleview-changed");

  info("Entering the commit key");
  EventUtils.synthesizeKey("KEY_Enter");
  await onRuleViewChanged;

  is(view._elementStyle.rules.length, 3, "Should have 3 rules.");
  ok(getRuleViewRule(view, name), "Rule with " + name + " selector exists.");
}

function checkModifiedElement(view, name) {
  is(view._elementStyle.rules.length, 2, "Should have 2 rules.");
  ok(getRuleViewRule(view, name), "Rule with " + name + " selector exists.");
}
