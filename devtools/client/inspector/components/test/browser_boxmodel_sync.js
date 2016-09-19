/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test editing box model syncs with the rule view.

const TEST_URI = "<p>hello</p>";

add_task(function* () {
  yield addTab("data:text/html," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openBoxModelView();

  info("When a property is edited, it should sync in the rule view");

  yield selectNode("p", inspector);

  info("Modify padding-bottom in box model view");
  let span = view.doc.querySelector(".boxmodel-padding.boxmodel-bottom > span");
  EventUtils.synthesizeMouseAtCenter(span, {}, view.doc.defaultView);
  let editor = view.doc.querySelector(".styleinspector-propertyeditor");

  EventUtils.synthesizeKey("7", {}, view.doc.defaultView);
  yield waitForUpdate(inspector);
  is(editor.value, "7", "Should have the right value in the editor.");
  EventUtils.synthesizeKey("VK_RETURN", {}, view.doc.defaultView);

  let onRuleViewRefreshed = once(inspector, "rule-view-refreshed");
  let onRuleViewSelected = once(inspector.sidebar, "ruleview-selected");
  info("Select the rule view and check that the property was synced there");
  let ruleView = selectRuleView(inspector);

  info("Wait for the rule view to be selected");
  yield onRuleViewSelected;

  info("Wait for the rule view to be refreshed");
  yield onRuleViewRefreshed;
  ok(true, "The rule view was refreshed");

  let ruleEditor = getRuleViewRuleEditor(ruleView, 0);
  let textProp = ruleEditor.rule.textProps[0];
  is(textProp.value, "7px", "The property has the right value");
});
