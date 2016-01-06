/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test to ensure that CSSOM doesn't make the rule view blow up.
// https://bugzilla.mozilla.org/show_bug.cgi?id=1224121

const TEST_URI = TEST_URL_ROOT + "doc_cssom.html";

add_task(function*() {
  yield addTab(TEST_URI);
  let {inspector, view} = yield openRuleView();
  yield selectNode("#target", inspector);

  let elementStyle = view._elementStyle;
  let rule = elementStyle.rules[1];

  is(rule.textProps.length, 1, "rule should have one property");
  is(rule.textProps[0].name, "color", "the property should be 'color'");
});
