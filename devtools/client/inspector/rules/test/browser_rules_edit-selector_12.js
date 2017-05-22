/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test editing selectors for rules inside @import'd stylesheets.
// This is a regression test for bug 1355819.

add_task(function* () {
  yield addTab(URL_ROOT + "doc_edit_imported_selector.html");
  let {inspector, view} = yield openRuleView();

  info("Select the node styled by an @import'd rule");
  yield selectNode("#target", inspector);

  info("Focus the selector in the rule-view");
  let ruleEditor = getRuleViewRuleEditor(view, 1);
  let editor = yield focusEditableField(view, ruleEditor.selectorText);

  info("Change the selector to something else");
  editor.input.value = "div";
  let onRuleViewChanged = once(view, "ruleview-changed");
  EventUtils.synthesizeKey("VK_RETURN", {});
  yield onRuleViewChanged;

  info("Escape the new property editor after editing the selector");
  let onBlur = once(view.styleDocument.activeElement, "blur");
  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.styleWindow);
  yield onBlur;

  info("Check the rules are still displayed correctly");
  is(view._elementStyle.rules.length, 3, "The element still has 3 rules.");

  ruleEditor = getRuleViewRuleEditor(view, 1);
  is(ruleEditor.element.getAttribute("unmatched"), "false", "Rule editor is matched.");
  is(ruleEditor.selectorText.textContent, "div", "The new selector is correct");
});
