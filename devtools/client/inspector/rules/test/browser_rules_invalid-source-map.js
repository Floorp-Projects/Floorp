/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that when a source map is missing/invalid, the rule view still loads
// correctly.

const TESTCASE_URI = URL_ROOT + "doc_invalid_sourcemap.html";
const PREF = "devtools.styleeditor.source-maps-enabled";
const CSS_LOC = "doc_invalid_sourcemap.css:1";

add_task(function* () {
  Services.prefs.setBoolPref(PREF, true);

  yield addTab(TESTCASE_URI);
  let {inspector, view} = yield openRuleView();

  yield selectNode("div", inspector);

  let ruleEl = getRuleViewRule(view, "div");
  ok(ruleEl, "The 'div' rule exists in the rule-view");

  let prop = getRuleViewProperty(view, "div", "color");
  ok(prop, "The 'color' property exists in this rule");

  let value = getRuleViewPropertyValue(view, "div", "color");
  is(value, "gold", "The 'color' property has the right value");

  yield verifyLinkText(view, CSS_LOC);

  Services.prefs.clearUserPref(PREF);
});

function verifyLinkText(view, text) {
  info("Verifying that the rule-view stylesheet link is " + text);
  let label = getRuleViewLinkByIndex(view, 1).querySelector("label");
  return waitForSuccess(
    () => label.getAttribute("value") == text,
    "Link text changed to display correct location: " + text
  );
}
