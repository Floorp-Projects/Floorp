/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test editing box model syncs with the rule view.

const TEST_URI = "<p>hello</p>";

add_task(async function() {
  await addTab("data:text/html," + encodeURIComponent(TEST_URI));
  const {inspector, boxmodel} = await openLayoutView();

  info("When a property is edited, it should sync in the rule view");

  await selectNode("p", inspector);

  info("Modify padding-bottom in box model view");
  const span =
    boxmodel.document.querySelector(".boxmodel-padding.boxmodel-bottom > span");
  EventUtils.synthesizeMouseAtCenter(span, {}, boxmodel.document.defaultView);
  const editor = boxmodel.document.querySelector(".styleinspector-propertyeditor");

  const onRuleViewRefreshed = once(inspector, "rule-view-refreshed");
  EventUtils.synthesizeKey("7", {}, boxmodel.document.defaultView);
  await waitForUpdate(inspector);
  await onRuleViewRefreshed;
  is(editor.value, "7", "Should have the right value in the editor.");
  EventUtils.synthesizeKey("VK_RETURN", {}, boxmodel.document.defaultView);

  info("Check that the property was synced with the rule view");
  const ruleView = selectRuleView(inspector);
  const ruleEditor = getRuleViewRuleEditor(ruleView, 0);
  const textProp = ruleEditor.rule.textProps[0];
  is(textProp.value, "7px", "The property has the right value");
});
