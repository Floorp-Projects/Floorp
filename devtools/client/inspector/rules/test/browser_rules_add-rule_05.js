/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the behaviour of adding a new rule to the rule view using the context
// menu and the various inplace-editor behaviours in the new rule editor.

const TEST_URI = `
  <style type="text/css">
    .testclass {
      text-align: center;
    }
  </style>
  <div id="testid" class="testclass">Styled Node</div>
  <span class="testclass2">This is a span</span>
  <span class="class1 class2">Multiple classes</span>
  <span class="class3      class4">Multiple classes</span>
  <p>Empty<p>
  <h1 class="asd@@@@a!!!!:::@asd">Invalid characters in class</h1>
  <h2 id="asd@@@a!!2a">Invalid characters in id</h2>
`;

const TEST_DATA = [
  { node: "#testid", expected: "#testid" },
  { node: ".testclass2", expected: ".testclass2" },
  { node: ".class1.class2", expected: ".class1.class2" },
  { node: ".class3.class4", expected: ".class3.class4" },
  { node: "p", expected: "p" },
  { node: "h1", expected: ".asd\\@\\@\\@\\@a\\!\\!\\!\\!\\:\\:\\:\\@asd" },
  { node: "h2", expected: "#asd\\@\\@\\@a\\!\\!2a" }
];

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();

  for (let data of TEST_DATA) {
    let {node, expected} = data;
    yield selectNode(node, inspector);
    yield testNewRule(inspector, view, expected);
  }
});

function* testNewRule(inspector, view, expected) {
  yield addNewRuleAndDismissEditor(inspector, view, expected, 1);

  info("Adding new properties to new rule: " + expected);
  let ruleEditor = getRuleViewRuleEditor(view, 1);
  let onRuleViewChanged = view.once("ruleview-changed");
  ruleEditor.addProperty("font-weight", "bold", "");
  yield onRuleViewChanged;

  let textProps = ruleEditor.rule.textProps;
  let lastRule = textProps[textProps.length - 1];
  is(lastRule.name, "font-weight", "Last rule name is font-weight");
  is(lastRule.value, "bold", "Last rule value is bold");
}
