/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the a new CSS rule can be added using the context menu.

const TEST_URI = '<div id="testid">Test Node</div>';

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();

  yield selectNode("#testid", inspector);
  yield addNewRuleFromContextMenu(inspector, view);
  yield testNewRule(view);
});

function* addNewRuleFromContextMenu(inspector, view) {
  info("Waiting for context menu to be shown");

  let allMenuItems = openStyleContextMenuAndGetAllItems(view, view.element);
  let menuitemAddRule = allMenuItems.find(item => item.label ===
    STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.addNewRule"));

  ok(menuitemAddRule.visible, "Add rule is visible");

  info("Adding the new rule and expecting a ruleview-changed event");
  let onRuleViewChanged = view.once("ruleview-changed");
  menuitemAddRule.click();
  yield onRuleViewChanged;
}

function* testNewRule(view) {
  let ruleEditor = getRuleViewRuleEditor(view, 1);
  let editor = ruleEditor.selectorText.ownerDocument.activeElement;
  is(editor.value, "#testid", "Selector editor value is as expected");

  info("Escaping from the selector field the change");
  EventUtils.synthesizeKey("VK_ESCAPE", {});
}
