/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the rule-view behaves correctly when entering mutliple and/or
// unfinished properties/values in inplace-editors

let test = asyncTest(function*() {
  yield addTab("data:text/html;charset=utf-8,test rule view user changes");
  content.document.body.innerHTML = "<h1>Testing Multiple Properties</h1>";
  let {toolbox, inspector, view} = yield openRuleView();

  info("Creating the test element");
  let newElement = content.document.createElement("div");
  newElement.textContent = "Test Element";
  content.document.body.appendChild(newElement);
  yield selectNode("div", inspector);
  let ruleEditor = getRuleViewRuleEditor(view, 0);

  yield testMultiValues(inspector, ruleEditor, view);
});

function* testMultiValues(inspector, ruleEditor, view) {
  yield createNewRuleViewProperty(ruleEditor, "width:");

  is(ruleEditor.rule.textProps.length, 1, "Should have created a new text property.");
  is(ruleEditor.propertyList.children.length, 1, "Should have created a property editor.");

  // Value is focused, lets add multiple rules here and make sure they get added
  let valueEditor = ruleEditor.propertyList.children[0].querySelector("input");
  valueEditor.value = "height: 10px;color:blue"
  EventUtils.synthesizeKey("VK_RETURN", {}, view.doc.defaultView);

  is(ruleEditor.rule.textProps.length, 2, "Should have added the changed value.");
  is(ruleEditor.propertyList.children.length, 3, "Should have added the changed value editor.");

  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.doc.defaultView);
  is(ruleEditor.propertyList.children.length, 2, "Should have removed the value editor.");

  is(ruleEditor.rule.textProps[0].name, "width", "Should have correct property name");
  is(ruleEditor.rule.textProps[0].value, "height: 10px", "Should have correct property value");

  is(ruleEditor.rule.textProps[1].name, "color", "Should have correct property name");
  is(ruleEditor.rule.textProps[1].value, "blue", "Should have correct property value");

  yield inspector.once("inspector-updated");
}
