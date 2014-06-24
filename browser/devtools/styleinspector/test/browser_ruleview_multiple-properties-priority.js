/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the rule-view behaves correctly when entering mutliple and/or
// unfinished properties/values in inplace-editors

let test = asyncTest(function*() {
  yield addTab("data:text/html,test rule view user changes");
  content.document.body.innerHTML = "<h1>Testing Multiple Properties</h1>";
  let {toolbox, inspector, view} = yield openRuleView();

  info("Creating the test element");
  let newElement = content.document.createElement("div");
  newElement.textContent = "Test Element";
  content.document.body.appendChild(newElement);
  yield selectNode(newElement, inspector);
  let ruleEditor = getRuleViewRuleEditor(view, 0);

  yield testCreateNewMultiPriority(inspector, ruleEditor);
});

function* testCreateNewMultiPriority(inspector, ruleEditor) {
  yield createNewRuleViewProperty(ruleEditor,
    "color:red;width:100px;height: 100px;");

  is(ruleEditor.rule.textProps.length, 3, "Should have created new text properties.");
  is(ruleEditor.propertyList.children.length, 4, "Should have created new property editors.");

  is(ruleEditor.rule.textProps[0].name, "color", "Should have correct property name");
  is(ruleEditor.rule.textProps[0].value, "red", "Should have correct property value");

  is(ruleEditor.rule.textProps[1].name, "width", "Should have correct property name");
  is(ruleEditor.rule.textProps[1].value, "100px", "Should have correct property value");

  is(ruleEditor.rule.textProps[2].name, "height", "Should have correct property name");
  is(ruleEditor.rule.textProps[2].value, "100px", "Should have correct property value");

  yield inspector.once("inspector-updated");
}
