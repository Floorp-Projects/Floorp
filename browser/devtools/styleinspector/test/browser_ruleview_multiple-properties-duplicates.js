/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the rule-view behaves correctly when entering mutliple and/or
// unfinished properties/values in inplace-editors

const TEST_URI = "<div>Test Element</div>";

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("div", inspector);
  yield testCreateNewMultiDuplicates(inspector, view);
});

function* testCreateNewMultiDuplicates(inspector, view) {
  let ruleEditor = getRuleViewRuleEditor(view, 0);
  let onMutation = inspector.once("markupmutation");
  yield createNewRuleViewProperty(ruleEditor,
    "color:red;color:orange;color:yellow;color:green;color:blue;color:indigo;" +
    "color:violet;");
  yield onMutation;

  is(ruleEditor.rule.textProps.length, 7,
    "Should have created new text properties.");
  is(ruleEditor.propertyList.children.length, 8,
    "Should have created new property editors.");

  is(ruleEditor.rule.textProps[0].name, "color",
    "Should have correct property name");
  is(ruleEditor.rule.textProps[0].value, "red",
    "Should have correct property value");

  is(ruleEditor.rule.textProps[1].name, "color",
    "Should have correct property name");
  is(ruleEditor.rule.textProps[1].value, "orange",
    "Should have correct property value");

  is(ruleEditor.rule.textProps[2].name, "color",
    "Should have correct property name");
  is(ruleEditor.rule.textProps[2].value, "yellow",
    "Should have correct property value");

  is(ruleEditor.rule.textProps[3].name, "color",
    "Should have correct property name");
  is(ruleEditor.rule.textProps[3].value, "green",
    "Should have correct property value");

  is(ruleEditor.rule.textProps[4].name, "color",
    "Should have correct property name");
  is(ruleEditor.rule.textProps[4].value, "blue",
    "Should have correct property value");

  is(ruleEditor.rule.textProps[5].name, "color",
    "Should have correct property name");
  is(ruleEditor.rule.textProps[5].value, "indigo",
    "Should have correct property value");

  is(ruleEditor.rule.textProps[6].name, "color",
    "Should have correct property name");
  is(ruleEditor.rule.textProps[6].value, "violet",
    "Should have correct property value");
}
