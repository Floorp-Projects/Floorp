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
  let ruleEditor = view.element.children[0]._ruleEditor;

  yield testCreateNewMulti(inspector, ruleEditor);
});

function* testCreateNewMulti(inspector, ruleEditor) {
  yield createNewRuleViewProperty(ruleEditor,
    "color:blue;background : orange   ; text-align:center; border-color: green;");

  is(ruleEditor.rule.textProps.length, 4, "Should have created a new text property.");
  is(ruleEditor.propertyList.children.length, 5, "Should have created a new property editor.");

  is(ruleEditor.rule.textProps[0].name, "color", "Should have correct property name");
  is(ruleEditor.rule.textProps[0].value, "blue", "Should have correct property value");

  is(ruleEditor.rule.textProps[1].name, "background", "Should have correct property name");
  is(ruleEditor.rule.textProps[1].value, "orange", "Should have correct property value");

  is(ruleEditor.rule.textProps[2].name, "text-align", "Should have correct property name");
  is(ruleEditor.rule.textProps[2].value, "center", "Should have correct property value");

  is(ruleEditor.rule.textProps[3].name, "border-color", "Should have correct property name");
  is(ruleEditor.rule.textProps[3].value, "green", "Should have correct property value");

  yield inspector.once("inspector-updated");
}
