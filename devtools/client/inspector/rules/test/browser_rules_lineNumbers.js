/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that editing a rule will update the line numbers of subsequent
// rules in the rule view.

const TESTCASE_URI = URL_ROOT + "doc_ruleLineNumbers.html";

add_task(async function() {
  await addTab(TESTCASE_URI);
  const { inspector, view } = await openRuleView();
  await selectNode("#testid", inspector);

  const bodyRuleEditor = getRuleViewRuleEditor(view, 3);
  const value = getRuleViewLinkTextByIndex(view, 2);
  // Note that this is relative to the <style>.
  is(value.slice(-2), ":6", "initial rule line number is 6");

  const onLocationChanged = once(bodyRuleEditor.rule.domRule, "location-changed");
  await addProperty(view, 1, "font-size", "23px");
  await onLocationChanged;

  const newBodyTitle = getRuleViewLinkTextByIndex(view, 2);
  // Note that this is relative to the <style>.
  is(newBodyTitle.slice(-2), ":7", "updated rule line number is 7");
});
