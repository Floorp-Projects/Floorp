/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that adding a new property of an unmatched rule works properly.

const TEST_URI = `
  <style type="text/css">
    #testid {
    }
    .testclass {
      background-color: white;
    }
  </style>
  <div id="testid">Styled Node</div>
  <span class="testclass">This is a span</span>
`;

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();

  info("Selecting the test element");
  await selectNode("#testid", inspector);
  await testEditSelector(view, "span");
  await testAddProperty(view);

  info("Selecting the modified element with the new rule");
  await selectNode("span", inspector);
  await checkModifiedElement(view, "span");
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

  info("Waiting for rule view to update");
  const onRuleViewChanged = once(view, "ruleview-changed");

  info("Entering the commit key");
  EventUtils.synthesizeKey("KEY_Enter");
  await onRuleViewChanged;

  is(view._elementStyle.rules.length, 2, "Should have 2 rules.");
  ok(getRuleViewRule(view, name), "Rule with " + name + " selector exists.");
  ok(
    getRuleViewRuleEditor(view, 1).element.getAttribute("unmatched"),
    "Rule with " + name + " does not match the current element."
  );
}

function checkModifiedElement(view, name) {
  is(view._elementStyle.rules.length, 3, "Should have 3 rules.");
  ok(getRuleViewRule(view, name), "Rule with " + name + " selector exists.");
}

async function testAddProperty(view) {
  info("Test creating a new property");
  const textProp = await addProperty(view, 1, "text-align", "center");

  is(textProp.value, "center", "Text prop should have been changed.");
  ok(!textProp.overridden, "Property should not be overridden");
}
