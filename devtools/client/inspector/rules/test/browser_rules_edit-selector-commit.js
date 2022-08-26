/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test selector value is correctly displayed when committing the inplace editor
// with ENTER, ESC, SHIFT+TAB and TAB

const TEST_URI = `
  <style type='text/css'>
    #testid1 {
      text-align: center;
    }
    #testid2 {
      text-align: center;
    }
    #testid3 {
    }
  </style>
  <div id='testid1'>Styled Node</div>
  <div id='testid2'>Styled Node</div>
  <div id='testid3'>Styled Node</div>
`;

const TEST_DATA = [
  {
    node: "#testid1",
    value: ".testclass",
    commitKey: "VK_ESCAPE",
    modifiers: {},
    expected: "#testid1",
  },
  {
    node: "#testid1",
    value: ".testclass1",
    commitKey: "VK_RETURN",
    modifiers: {},
    expected: ".testclass1",
  },
  {
    node: "#testid2",
    value: ".testclass2",
    commitKey: "VK_TAB",
    modifiers: {},
    expected: ".testclass2",
  },
  {
    node: "#testid3",
    value: ".testclass3",
    commitKey: "VK_TAB",
    modifiers: { shiftKey: true },
    expected: ".testclass3",
  },
];

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();

  for (const data of TEST_DATA) {
    await runTestData(inspector, view, data);
  }
});

async function runTestData(inspector, view, data) {
  const { node, value, commitKey, modifiers, expected } = data;

  info(
    "Updating " +
      node +
      " to " +
      value +
      " and committing with " +
      commitKey +
      ". Expecting: " +
      expected
  );

  info("Selecting the test element");
  await selectNode(node, inspector);

  let idRuleEditor = getRuleViewRuleEditor(view, 1);

  info("Focusing an existing selector name in the rule-view");
  const editor = await focusEditableField(view, idRuleEditor.selectorText);
  is(
    inplaceEditor(idRuleEditor.selectorText),
    editor,
    "The selector editor got focused"
  );

  info("Enter the new selector value: " + value);
  editor.input.value = value;

  info("Entering the commit key " + commitKey + " " + modifiers);
  EventUtils.synthesizeKey(commitKey, modifiers);

  const activeElement = view.styleDocument.activeElement;

  if (commitKey === "VK_ESCAPE") {
    is(
      idRuleEditor.rule.selectorText,
      expected,
      "Value is as expected: " + expected
    );
    is(idRuleEditor.isEditing, false, "Selector is not being edited.");
    is(idRuleEditor.selectorText, activeElement, "Focus is on selector span.");
    return;
  }

  await once(view, "ruleview-changed");

  ok(
    getRuleViewRule(view, expected),
    "Rule with " + expected + " selector exists."
  );

  if (modifiers.shiftKey) {
    idRuleEditor = getRuleViewRuleEditor(view, 0);
  }

  const rule = idRuleEditor.rule;
  if (rule.textProps.length) {
    is(
      inplaceEditor(rule.textProps[0].editor.nameSpan).input,
      activeElement,
      "Focus is on the first property name span."
    );
  } else {
    is(
      inplaceEditor(idRuleEditor.newPropSpan).input,
      activeElement,
      "Focus is on the new property span."
    );
  }
}
