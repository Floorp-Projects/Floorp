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

  yield testCreateNewMultiDuplicates(inspector, ruleEditor);
});

function* testCreateNewMultiDuplicates(inspector, ruleEditor) {
  yield createNewRuleViewProperty(ruleEditor,
    "color:red;color:orange;color:yellow;color:green;color:blue;color:indigo;color:violet;");

  is(ruleEditor.rule.textProps.length, 7, "Should have created new text properties.");
  is(ruleEditor.propertyList.children.length, 8, "Should have created new property editors.");

  is(ruleEditor.rule.textProps[0].name, "color", "Should have correct property name");
  is(ruleEditor.rule.textProps[0].value, "red", "Should have correct property value");

  is(ruleEditor.rule.textProps[1].name, "color", "Should have correct property name");
  is(ruleEditor.rule.textProps[1].value, "orange", "Should have correct property value");

  is(ruleEditor.rule.textProps[2].name, "color", "Should have correct property name");
  is(ruleEditor.rule.textProps[2].value, "yellow", "Should have correct property value");

  is(ruleEditor.rule.textProps[3].name, "color", "Should have correct property name");
  is(ruleEditor.rule.textProps[3].value, "green", "Should have correct property value");

  is(ruleEditor.rule.textProps[4].name, "color", "Should have correct property name");
  is(ruleEditor.rule.textProps[4].value, "blue", "Should have correct property value");

  is(ruleEditor.rule.textProps[5].name, "color", "Should have correct property name");
  is(ruleEditor.rule.textProps[5].value, "indigo", "Should have correct property value");

  is(ruleEditor.rule.textProps[6].name, "color", "Should have correct property name");
  is(ruleEditor.rule.textProps[6].value, "violet", "Should have correct property value");

  yield inspector.once("inspector-updated");
}