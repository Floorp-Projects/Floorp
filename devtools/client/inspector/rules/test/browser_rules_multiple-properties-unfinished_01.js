/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the rule-view behaves correctly when entering multiple and/or
// unfinished properties/values in inplace-editors

const TEST_URI = "<div>Test Element</div>";

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("div", inspector);
  yield testCreateNewMultiUnfinished(inspector, view);
});

function* testCreateNewMultiUnfinished(inspector, view) {
  let ruleEditor = getRuleViewRuleEditor(view, 0);
  let onMutation = inspector.once("markupmutation");
  // There are 2 rule-view updates: one for the preview and one for
  // the final commit.
  let onRuleViewChanged = waitForNEvents(view, "ruleview-changed", 2);
  yield createNewRuleViewProperty(ruleEditor,
    "color:blue;background : orange   ; text-align:center; border-color: ");
  yield onMutation;
  yield onRuleViewChanged;

  is(ruleEditor.rule.textProps.length, 4,
    "Should have created new text properties.");
  is(ruleEditor.propertyList.children.length, 4,
    "Should have created property editors.");

  EventUtils.sendString("red", view.styleWindow);
  onRuleViewChanged = view.once("ruleview-changed");
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);
  yield onRuleViewChanged;

  is(ruleEditor.rule.textProps.length, 4,
    "Should have the same number of text properties.");
  is(ruleEditor.propertyList.children.length, 5,
    "Should have added the changed value editor.");

  is(ruleEditor.rule.textProps[0].name, "color",
    "Should have correct property name");
  is(ruleEditor.rule.textProps[0].value, "blue",
    "Should have correct property value");

  is(ruleEditor.rule.textProps[1].name, "background",
    "Should have correct property name");
  is(ruleEditor.rule.textProps[1].value, "orange",
    "Should have correct property value");

  is(ruleEditor.rule.textProps[2].name, "text-align",
    "Should have correct property name");
  is(ruleEditor.rule.textProps[2].value, "center",
    "Should have correct property value");

  is(ruleEditor.rule.textProps[3].name, "border-color",
    "Should have correct property name");
  is(ruleEditor.rule.textProps[3].value, "red",
    "Should have correct property value");
}
