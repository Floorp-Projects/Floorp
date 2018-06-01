/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that when a source map comment appears in an inline stylesheet, the
// rule-view still appears correctly.
// Bug 1255787.

const TESTCASE_URI = URL_ROOT + "doc_inline_sourcemap.html";
const PREF = "devtools.source-map.client-service.enabled";

add_task(async function() {
  Services.prefs.setBoolPref(PREF, true);

  await addTab(TESTCASE_URI);
  const {inspector, view} = await openRuleView();

  await selectNode("div", inspector);

  const ruleEl = getRuleViewRule(view, "div");
  ok(ruleEl, "The 'div' rule exists in the rule-view");

  Services.prefs.clearUserPref(PREF);
});
