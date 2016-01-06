/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that editing a rule will update the line numbers of subsequent
// rules in the rule view.

const TESTCASE_URI = TEST_URL_ROOT + "doc_keyframeLineNumbers.html";

add_task(function*() {
  yield addTab(TESTCASE_URI);
  let { inspector, view } = yield openRuleView();
  yield selectNode("#outer", inspector);

  // Insert a new property, which will affect the line numbers.
  let elementRuleEditor = getRuleViewRuleEditor(view, 1);
  yield createNewRuleViewProperty(elementRuleEditor, "font-size: 72px");

  let onRefresh = view.once("ruleview-refreshed");
  yield selectNode("#inner", inspector);
  yield onRefresh;

  let value = getRuleViewLinkTextByIndex(view, 3);
  // Note that this is relative to the <style>.
  is(value.slice(-3), ":27", "rule line number is 27");
});
