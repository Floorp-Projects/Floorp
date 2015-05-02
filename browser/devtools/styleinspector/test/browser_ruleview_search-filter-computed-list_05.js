/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view search filter works properly in the computed list
// for parsed property name.

const SEARCH = "margin-top:"

let TEST_URI = [
  '<style type="text/css">',
  '  #testid {',
  '    margin: 4px 0px;',
  '  }',
  '  .testclass {',
  '    background-color: red;',
  '  }',
  '</style>',
  '<h1 id="testid" class="testclass">Styled Node</h1>'
].join("\n");

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {toolbox, inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);
  yield testAddTextInFilter(inspector, view);
});

function* testAddTextInFilter(inspector, ruleView) {
  info("Setting filter text to \"" + SEARCH + "\"");

  let win = ruleView.doc.defaultView;
  let searchField = ruleView.searchField;
  let onRuleViewFiltered = inspector.once("ruleview-filtered");

  searchField.focus();
  synthesizeKeys(SEARCH, win);
  yield onRuleViewFiltered;

  info("Check that the correct rules are visible");
  is(ruleView.element.children.length, 2, "Should have 2 rules.");
  is(getRuleViewRuleEditor(ruleView, 0).rule.selectorText, "element",
    "First rule is inline element.");

  let rule = getRuleViewRuleEditor(ruleView, 1).rule;
  let ruleEditor = rule.textProps[0].editor;
  let computed = ruleEditor.computed;

  is(rule.selectorText, "#testid", "Second rule is #testid.");
  ok(ruleEditor.expander.getAttribute("open"), "Expander is open.");
  ok(!ruleEditor.container.classList.contains("ruleview-highlight"),
    "margin text property is not highlighted.");
  ok(computed.hasAttribute("filter-open"), "margin computed list is open.");

  ok(computed.children[0].classList.contains("ruleview-highlight"),
    "margin-top computed property is correctly highlighted.");
  ok(!computed.children[1].classList.contains("ruleview-highlight"),
    "margin-right computed property is not highlighted.");
  ok(!computed.children[2].classList.contains("ruleview-highlight"),
    "margin-bottom computed property is not highlighted.");
  ok(!computed.children[3].classList.contains("ruleview-highlight"),
    "margin-left computed property is not highlighted.");
}
