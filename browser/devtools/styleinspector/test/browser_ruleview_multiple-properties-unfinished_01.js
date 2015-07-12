/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the rule-view behaves correctly when entering mutliple and/or
// unfinished properties/values in inplace-editors

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8,test rule view user changes");
  content.document.body.innerHTML = "<h1>Testing Multiple Properties</h1>";
  let {toolbox, inspector, view} = yield openRuleView();

  info("Creating the test element");
  let newElement = content.document.createElement("div");
  newElement.textContent = "Test Element";
  content.document.body.appendChild(newElement);
  yield selectNode("div", inspector);
  let ruleEditor = getRuleViewRuleEditor(view, 0);

  yield testCreateNewMultiUnfinished(inspector, ruleEditor, view);
});

function waitRuleViewChanged(view, n) {
  let deferred = promise.defer();
  let count = 0;
  let listener = function () {
    if (++count == n) {
      view.off("ruleview-changed", listener);
      deferred.resolve();
    }
  }
  view.on("ruleview-changed", listener);
  return deferred.promise;
}
function* testCreateNewMultiUnfinished(inspector, ruleEditor, view) {
  let onMutation = inspector.once("markupmutation");
  // There is 6 rule-view updates, one for the rule view creation,
  // one for each new property and one last for throttle update.
  let onRuleViewChanged = waitRuleViewChanged(view, 6);
  yield createNewRuleViewProperty(ruleEditor,
    "color:blue;background : orange   ; text-align:center; border-color: ");
  yield onMutation;
  yield onRuleViewChanged;

  is(ruleEditor.rule.textProps.length, 4, "Should have created new text properties.");
  is(ruleEditor.propertyList.children.length, 4, "Should have created property editors.");

  EventUtils.sendString("red", view.styleWindow);
  onRuleViewChanged = view.once("ruleview-changed");
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);
  yield onRuleViewChanged;

  is(ruleEditor.rule.textProps.length, 4, "Should have the same number of text properties.");
  is(ruleEditor.propertyList.children.length, 5, "Should have added the changed value editor.");

  is(ruleEditor.rule.textProps[0].name, "color", "Should have correct property name");
  is(ruleEditor.rule.textProps[0].value, "blue", "Should have correct property value");

  is(ruleEditor.rule.textProps[1].name, "background", "Should have correct property name");
  is(ruleEditor.rule.textProps[1].value, "orange", "Should have correct property value");

  is(ruleEditor.rule.textProps[2].name, "text-align", "Should have correct property name");
  is(ruleEditor.rule.textProps[2].value, "center", "Should have correct property value");

  is(ruleEditor.rule.textProps[3].name, "border-color", "Should have correct property name");
  is(ruleEditor.rule.textProps[3].value, "red", "Should have correct property value");
}
