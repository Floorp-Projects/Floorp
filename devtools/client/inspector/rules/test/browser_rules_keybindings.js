/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that focus doesn't leave the style editor when adding a property
// (bug 719916)

add_task(async function() {
  await addTab("data:text/html;charset=utf-8,<h1>Some header text</h1>");
  const {inspector, view} = await openRuleView();
  await selectNode("h1", inspector);

  info("Getting the ruleclose brace element");
  const brace = view.styleDocument.querySelector(".ruleview-ruleclose");

  info("Focus the new property editable field to create a color property");
  const ruleEditor = getRuleViewRuleEditor(view, 0);
  let editor = await focusNewRuleViewProperty(ruleEditor);
  editor.input.value = "color";

  info("Typing ENTER to focus the next field: property value");
  let onFocus = once(brace.parentNode, "focus", true);
  let onRuleViewChanged = view.once("ruleview-changed");

  EventUtils.sendKey("return");

  await onFocus;
  await onRuleViewChanged;
  ok(true, "The value field was focused");

  info("Entering a property value");
  editor = getCurrentInplaceEditor(view);
  editor.input.value = "green";

  info("Typing ENTER again should focus a new property name");
  onFocus = once(brace.parentNode, "focus", true);
  onRuleViewChanged = view.once("ruleview-changed");
  EventUtils.sendKey("return");
  await onFocus;
  await onRuleViewChanged;
  ok(true, "The new property name field was focused");
  getCurrentInplaceEditor(view).input.blur();
});

function getCurrentInplaceEditor(view) {
  return inplaceEditor(view.styleDocument.activeElement);
}
