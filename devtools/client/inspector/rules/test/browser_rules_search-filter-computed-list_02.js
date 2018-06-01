/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view search filter works properly in the computed list
// when modifying the existing search filter value

const SEARCH = "margin-";

const TEST_URI = `
  <style type="text/css">
    #testid {
      margin: 4px 0px;
    }
    .testclass {
      background-color: red;
    }
  </style>
  <h1 id="testid" class="testclass">Styled Node</h1>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();
  await selectNode("#testid", inspector);
  await testAddTextInFilter(inspector, view);
  await testRemoveTextInFilter(inspector, view);
});

async function testAddTextInFilter(inspector, view) {
  await setSearchFilter(view, SEARCH);

  info("Check that the correct rules are visible");
  is(view.element.children.length, 2, "Should have 2 rules.");
  is(getRuleViewRuleEditor(view, 0).rule.selectorText, "element",
    "First rule is inline element.");

  const rule = getRuleViewRuleEditor(view, 1).rule;
  const ruleEditor = rule.textProps[0].editor;
  const computed = ruleEditor.computed;

  is(rule.selectorText, "#testid", "Second rule is #testid.");
  ok(ruleEditor.expander.getAttribute("open"), "Expander is open.");
  ok(!ruleEditor.container.classList.contains("ruleview-highlight"),
    "margin text property is not highlighted.");
  ok(computed.hasAttribute("filter-open"), "margin computed list is open.");

  ok(computed.children[0].classList.contains("ruleview-highlight"),
    "margin-top computed property is correctly highlighted.");
  ok(computed.children[1].classList.contains("ruleview-highlight"),
    "margin-right computed property is correctly highlighted.");
  ok(computed.children[2].classList.contains("ruleview-highlight"),
    "margin-bottom computed property is correctly highlighted.");
  ok(computed.children[3].classList.contains("ruleview-highlight"),
    "margin-left computed property is correctly highlighted.");
}

async function testRemoveTextInFilter(inspector, view) {
  info("Press backspace and set filter text to \"margin\"");

  const win = view.styleWindow;
  const searchField = view.searchField;

  searchField.focus();
  EventUtils.synthesizeKey("VK_BACK_SPACE", {}, win);
  await inspector.once("ruleview-filtered");

  info("Check that the correct rules are visible");
  is(view.element.children.length, 2, "Should have 2 rules.");
  is(getRuleViewRuleEditor(view, 0).rule.selectorText, "element",
    "First rule is inline element.");

  const rule = getRuleViewRuleEditor(view, 1).rule;
  const ruleEditor = rule.textProps[0].editor;
  const computed = ruleEditor.computed;

  is(rule.selectorText, "#testid", "Second rule is #testid.");
  ok(!ruleEditor.expander.getAttribute("open"), "Expander is closed.");
  ok(ruleEditor.container.classList.contains("ruleview-highlight"),
    "margin text property is correctly highlighted.");
  ok(!computed.hasAttribute("filter-open"), "margin computed list is closed.");

  ok(computed.children[0].classList.contains("ruleview-highlight"),
    "margin-top computed property is correctly highlighted.");
  ok(computed.children[1].classList.contains("ruleview-highlight"),
    "margin-right computed property is correctly highlighted.");
  ok(computed.children[2].classList.contains("ruleview-highlight"),
    "margin-bottom computed property is correctly highlighted.");
  ok(computed.children[3].classList.contains("ruleview-highlight"),
    "margin-left computed property is correctly highlighted.");
}
