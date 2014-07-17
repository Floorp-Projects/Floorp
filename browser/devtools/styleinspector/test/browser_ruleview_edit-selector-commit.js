/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test selector value is correctly displayed when committing the inplace editor
// with ENTER, ESC, SHIFT+TAB and TAB

let PAGE_CONTENT = [
  '<style type="text/css">',
  '  #testid {',
  '    text-align: center;',
  '  }',
  '</style>',
  '<div id="testid" class="testclass">Styled Node</div>',
].join("\n");

const TEST_DATA = [
  {
    node: "#testid",
    value: ".testclass",
    commitKey: "VK_ESCAPE",
    modifiers: {},
    expected: "#testid"
  },
  {
    node: "#testid",
    value: ".testclass",
    commitKey: "VK_RETURN",
    modifiers: {},
    expected: ".testclass"
  },
  {
    node: "#testid",
    value: ".testclass",
    commitKey: "VK_TAB",
    modifiers: {},
    expected: ".testclass"
  },
  {
    node: "#testid",
    value: ".testclass",
    commitKey: "VK_TAB",
    modifiers: {shiftKey: true},
    expected: ".testclass"
  }
];

let test = asyncTest(function*() {
  yield addTab("data:text/html;charset=utf-8,test escaping selector change reverts back to original value");

  info("Creating the test document");
  content.document.body.innerHTML = PAGE_CONTENT;

  info("Opening the rule-view");
  let {toolbox, inspector, view} = yield openRuleView();

  info("Iterating over the test data");
  for (let data of TEST_DATA) {
    yield runTestData(inspector, view, data);
  }
});

function* runTestData(inspector, view, data) {
  let {node, value, commitKey, modifiers, expected} = data;

  info("Updating " + node + " to " + value + " and committing with " + commitKey + ". Expecting: " + expected);

  info("Selecting the test element");
  yield selectNode(node, inspector);

  let idRuleEditor = getRuleViewRuleEditor(view, 1);

  info("Focusing an existing selector name in the rule-view");
  let editor = yield focusEditableField(idRuleEditor.selectorText);
  is(inplaceEditor(idRuleEditor.selectorText), editor,
      "The selector editor got focused");

  info("Enter the new selector value: " + value);
  editor.input.value = value;

  info("Entering the commit key " + commitKey + " " + modifiers);
  EventUtils.synthesizeKey(commitKey, modifiers);

  if (commitKey === "VK_ESCAPE") {
    is(idRuleEditor.rule.selectorText, expected,
        "Value is as expected: " + expected);
    is(idRuleEditor.isEditing, false, "Selector is not being edited.")
  } else {
    yield once(view.element, "CssRuleViewRefreshed");
    ok(getRuleViewRule(view, expected),
        "Rule with " + name + " selector exists.");
  }

  info("Resetting page content");
  content.document.body.innerHTML = PAGE_CONTENT;
}
