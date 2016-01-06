/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that editing a rule will update the line numbers of subsequent
// rules in the rule view.

const TESTCASE_URI = TEST_URL_ROOT + "doc_ruleLineNumbers.html";

add_task(function*() {
  yield addTab(TESTCASE_URI);
  let { inspector, view } = yield openRuleView();
  yield selectNode("#testid", inspector);
  let elementRuleEditor = getRuleViewRuleEditor(view, 1);

  let bodyRuleEditor = getRuleViewRuleEditor(view, 3);
  let value = getRuleViewLinkTextByIndex(view, 2);
  // Note that this is relative to the <style>.
  is(value.slice(-2), ":6", "initial rule line number is 6");

  info("Focusing a new property name in the rule-view");
  let editor = yield focusEditableField(view, elementRuleEditor.closeBrace);

  is(inplaceEditor(elementRuleEditor.newPropSpan), editor,
    "The new property editor got focused");
  let input = editor.input;

  info("Entering font-size in the property name editor");
  input.value = "font-size";

  info("Pressing return to commit and focus the new value field");
  let onLocationChanged = once(bodyRuleEditor.rule.domRule, "location-changed");
  let onValueFocus = once(elementRuleEditor.element, "focus", true);
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);
  yield onValueFocus;
  yield elementRuleEditor.rule._applyingModifications;

  // Getting the new value editor after focus
  editor = inplaceEditor(view.styleDocument.activeElement);
  info("Entering a value and bluring the field to expect a rule change");
  editor.input.value = "23px";
  editor.input.blur();
  yield elementRuleEditor.rule._applyingModifications;

  yield onLocationChanged;

  let newBodyTitle = getRuleViewLinkTextByIndex(view, 2);
  // Note that this is relative to the <style>.
  is(newBodyTitle.slice(-2), ":7", "updated rule line number is 7");
});
