/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the rule-view behaves correctly when entering mutliple and/or
// unfinished properties/values in inplace-editors.

const TEST_URI = "<div>Test Element</div>";

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await selectNode("div", inspector);

  const ruleEditor = getRuleViewRuleEditor(view, 0);
  // Note that we wait for a markup mutation here because this new rule will end
  // up creating a style attribute on the node shown in the markup-view.
  // (we also wait for the rule-view to refresh).
  const onMutation = inspector.once("markupmutation");
  const onRuleViewChanged = view.once("ruleview-changed");
  await createNewRuleViewProperty(
    ruleEditor,
    "color:red;width:100px;height: 100px;"
  );
  await onMutation;
  await onRuleViewChanged;

  is(
    ruleEditor.rule.textProps.length,
    3,
    "Should have created new text properties."
  );
  is(
    ruleEditor.propertyList.children.length,
    4,
    "Should have created new property editors."
  );

  is(
    getTextProperty(view, 0, { color: "red" }).name,
    "color",
    "Should have correct property name"
  );
  is(
    getTextProperty(view, 0, { color: "red" }).value,
    "red",
    "Should have correct property value"
  );

  is(
    getTextProperty(view, 0, { width: "100px" }).name,
    "width",
    "Should have correct property name"
  );
  is(
    getTextProperty(view, 0, { width: "100px" }).value,
    "100px",
    "Should have correct property value"
  );

  is(
    getTextProperty(view, 0, { height: "100px" }).name,
    "height",
    "Should have correct property name"
  );
  is(
    getTextProperty(view, 0, { height: "100px" }).value,
    "100px",
    "Should have correct property value"
  );
});
