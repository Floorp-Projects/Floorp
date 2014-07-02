/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the behaviour of adding a new rule to the rule view and editing
// its selector

let PAGE_CONTENT = [
  '<style type="text/css">',
  '  #testid {',
  '    text-align: center;',
  '  }',
  '</style>',
  '<div id="testid">Styled Node</div>',
  '<span>This is a span</span>'
].join("\n");

let test = asyncTest(function*() {
  yield addTab("data:text/html;charset=utf-8,test rule view add rule");

  info("Creating the test document");
  content.document.body.innerHTML = PAGE_CONTENT;

  info("Opening the rule-view");
  let {toolbox, inspector, view} = yield openRuleView();

  info("Selecting the test element");
  yield selectNode("#testid", inspector);

  info("Waiting for context menu to be shown");
  let onPopup = once(view._contextmenu, "popupshown");
  let win = view.doc.defaultView;

  EventUtils.synthesizeMouseAtCenter(view.element,
    {button: 2, type: "contextmenu"}, win);
  yield onPopup;

  ok(!view.menuitemAddRule.hidden, "Add rule is visible");

  info("Waiting for rule view to change");
  let onRuleViewChanged = once(view.element, "CssRuleViewChanged");

  info("Adding the new rule");
  view.menuitemAddRule.click();
  yield onRuleViewChanged;
  view._contextmenu.hidePopup();

  yield testEditSelector(view, "span");

  info("Selecting the modified element");
  yield selectNode("span", inspector);
  yield checkModifiedElement(view, "span");
});

function* testEditSelector(view, name) {
  info("Test editing existing selector field");
  let idRuleEditor = getRuleViewRuleEditor(view, 1);
  let editor = idRuleEditor.selectorText.ownerDocument.activeElement;

  info("Entering a new selector name and committing");
  editor.value = name;

  info("Waiting for rule view to refresh");
  let onRuleViewRefresh = once(view.element, "CssRuleViewRefreshed");

  info("Entering the commit key");
  EventUtils.synthesizeKey("VK_RETURN", {});
  yield onRuleViewRefresh;

  is(view._elementStyle.rules.length, 2, "Should have 2 rules.");
}

function* checkModifiedElement(view, name) {
  is(view._elementStyle.rules.length, 2, "Should have 2 rules.");
  ok(getRuleViewRule(view, name), "Rule with " + name + " selector exists.");
}
