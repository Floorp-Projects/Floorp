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

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(PAGE_CONTENT));

  info("Opening the rule-view");
  let {toolbox, inspector, view} = yield openRuleView();

  info("Selecting the test element");
  yield selectNode("#testid", inspector);

  info("Waiting for rule view to change");
  let onRuleViewChanged = once(view, "ruleview-changed");

  info("Adding the new rule");
  view.addRuleButton.click();

  yield onRuleViewChanged;

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
  let onRuleViewRefresh = once(view, "ruleview-refreshed");

  info("Entering the commit key");
  EventUtils.synthesizeKey("VK_RETURN", {});
  yield onRuleViewRefresh;

  is(view._elementStyle.rules.length, 2, "Should have 2 rules.");
}

function* checkModifiedElement(view, name) {
  is(view._elementStyle.rules.length, 2, "Should have 2 rules.");
  ok(getRuleViewRule(view, name), "Rule with " + name + " selector exists.");
}
