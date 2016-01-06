/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the behaviour of adding a new rule to the rule view and the
// various inplace-editor behaviours in the new rule editor.

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

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();

  for (let data of TEST_DATA) {
    yield runTestData(inspector, view, data, "context-menu");
    yield runTestData(inspector, view, data, "button");
  }
});

function* runTestData(inspector, view, data, method) {
  let {node, expected} = data;
  yield selectNode(node, inspector);
  yield addNewRule(inspector, view, method);
  yield testNewRule(view, expected, 1);

  info("Resetting page content");
  content.document.body.innerHTML = TEST_URI;
}

function* addNewRule(inspector, view, method) {
  if (method == "context-menu") {
    info("Waiting for context menu to be shown");
    let onPopup = once(view._contextmenu._menupopup, "popupshown");
    let win = view.styleWindow;

    EventUtils.synthesizeMouseAtCenter(view.element,
      {button: 2, type: "contextmenu"}, win);
    yield onPopup;

    ok(!view._contextmenu.menuitemAddRule.hidden, "Add rule is visible");

    info("Adding the new rule");
    view._contextmenu.menuitemAddRule.click();
    view._contextmenu._menupopup.hidePopup();
  } else {
    info("Adding the new rule using the button");
    view.addRuleButton.click();
  }

  info("Waiting for rule view to change");
  yield view.once("ruleview-changed");
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

  info("Adding new properties to new rule: " + expected);
  let onRuleViewChanged = view.once("ruleview-changed");
  idRuleEditor.addProperty("font-weight", "bold", "");
  yield onRuleViewChanged;

  let textProps = idRuleEditor.rule.textProps;
  let lastRule = textProps[textProps.length - 1];
  is(lastRule.name, "font-weight", "Last rule name is font-weight");
  is(lastRule.value, "bold", "Last rule value is bold");
}
