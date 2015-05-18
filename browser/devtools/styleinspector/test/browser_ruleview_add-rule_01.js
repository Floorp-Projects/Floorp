/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the behaviour of adding a new rule to the rule view and the
// various inplace-editor behaviours in the new rule editor

let PAGE_CONTENT = [
  '<style type="text/css">',
  '  .testclass {',
  '    text-align: center;',
  '  }',
  '</style>',
  '<div id="testid" class="testclass">Styled Node</div>',
  '<span class="testclass2">This is a span</span>',
  '<span class="class1 class2">Multiple classes</span>',
  '<p>Empty<p>'
].join("\n");

const TEST_DATA = [
  { node: "#testid", expected: "#testid" },
  { node: ".testclass2", expected: ".testclass2" },
  { node: ".class1.class2", expected: ".class1" },
  { node: "p", expected: "p" }
];

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(PAGE_CONTENT));

  info("Opening the rule-view");
  let {toolbox, inspector, view} = yield openRuleView();

  info("Iterating over the test data");
  for (let data of TEST_DATA) {
    yield runTestData(inspector, view, data, "context-menu");
    yield runTestData(inspector, view, data, "button");
  }
});

function* runTestData(inspector, view, data, method) {
  let {node, expected} = data;
  info("Selecting the test element");
  yield selectNode(node, inspector);

  yield addNewRule(inspector, view, method);

  yield testNewRule(view, expected, 1);

  info("Resetting page content");
  content.document.body.innerHTML = PAGE_CONTENT;
}

function* addNewRule(inspector, view, method) {
  if (method == "context-menu") {
    info("Waiting for context menu to be shown");
    let onPopup = once(view._contextmenu, "popupshown");
    let win = view.doc.defaultView;

    EventUtils.synthesizeMouseAtCenter(view.element,
      {button: 2, type: "contextmenu"}, win);
    yield onPopup;

    ok(!view.menuitemAddRule.hidden, "Add rule is visible");

    info("Adding the new rule");
    view.menuitemAddRule.click();
    view._contextmenu.hidePopup();
  }
  else {
    info("Adding the new rule using the button");
    view.addRuleButton.click();
  }
  info("Waiting for rule view to change");
  let onRuleViewChanged = once(view, "ruleview-changed");
  yield onRuleViewChanged;
}

function* testNewRule(view, expected, index) {
  let idRuleEditor = getRuleViewRuleEditor(view, index);
  let editor = idRuleEditor.selectorText.ownerDocument.activeElement;
  is(editor.value, expected,
      "Selector editor value is as expected: " + expected);

  info("Entering the escape key");
  EventUtils.synthesizeKey("VK_ESCAPE", {});

  is(idRuleEditor.selectorText.textContent, expected,
      "Selector text value is as expected: " + expected);

  info("Adding new properties to new rule: " + expected)
  idRuleEditor.addProperty("font-weight", "bold", "");
  let textProps = idRuleEditor.rule.textProps;
  let lastRule = textProps[textProps.length - 1];
  is(lastRule.name, "font-weight", "Last rule name is font-weight");
  is(lastRule.value, "bold", "Last rule value is bold");
}