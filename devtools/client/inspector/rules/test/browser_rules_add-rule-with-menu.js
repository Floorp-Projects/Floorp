/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the a new CSS rule can be added using the context menu.

const TEST_URI = '<div id="testid">Test Node</div>';

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();

  await selectNode("#testid", inspector);
  await addNewRuleFromContextMenu(inspector, view);
  await testNewRule(view);
});

async function addNewRuleFromContextMenu(inspector, view) {
  info("Waiting for context menu to be shown");

  const allMenuItems = openStyleContextMenuAndGetAllItems(view, view.element);
  const menuitemAddRule = allMenuItems.find(item => item.label ===
    STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.addNewRule"));

  ok(menuitemAddRule.visible, "Add rule is visible");

  info("Adding the new rule and expecting a ruleview-changed event");
  const onRuleViewChanged = view.once("ruleview-changed");
  menuitemAddRule.click();
  await onRuleViewChanged;
}

function testNewRule(view) {
  const ruleEditor = getRuleViewRuleEditor(view, 1);
  const editor = ruleEditor.selectorText.ownerDocument.activeElement;
  is(editor.value, "#testid", "Selector editor value is as expected");

  info("Escaping from the selector field the change");
  EventUtils.synthesizeKey("KEY_Escape");
}
