/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view shows expanders for properties with computed lists.

var TEST_URI = `
  <style type="text/css">
    #testid {
      margin: 4px;
      top: 0px;
    }
  </style>
  <h1 id="testid">Styled Node</h1>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);
  yield testExpandersShown(inspector, view);
});

function* testExpandersShown(inspector, view) {
  let rule = getRuleViewRuleEditor(view, 1).rule;

  info("Check that the correct rules are visible");
  is(rule.selectorText, "#testid", "Second rule is #testid.");
  is(rule.textProps[0].name, "margin", "First property is margin.");
  is(rule.textProps[1].name, "top", "Second property is top.");

  info("Check that the expanders are shown correctly");
  is(rule.textProps[0].editor.expander.style.visibility, "visible",
      "margin expander is visible.");
  is(rule.textProps[1].editor.expander.style.visibility, "hidden",
      "top expander is hidden.");
  ok(!rule.textProps[0].editor.expander.hasAttribute("open"),
      "margin computed list is closed.");
  ok(!rule.textProps[1].editor.expander.hasAttribute("open"),
      "top computed list is closed.");
  ok(!rule.textProps[0].editor.computed.hasChildNodes(),
      "margin computed list is empty before opening.");
  ok(!rule.textProps[1].editor.computed.hasChildNodes(),
      "top computed list is empty.");
}
