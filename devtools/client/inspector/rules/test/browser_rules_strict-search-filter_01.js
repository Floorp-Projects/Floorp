/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view strict search filter works properly for property
// names.

const TEST_URI = `
  <style type="text/css">
    #testid {
      width: 2%;
      color: red;
    }
    .testclass {
      width: 22%;
      background-color: #00F;
    }
  </style>
  <h1 id="testid" class="testclass">Styled Node</h1>
`;

const TEST_DATA = [
  {
    desc: "Tests that the strict search filter works properly for property " +
          "names",
    search: "`color`",
    ruleCount: 2,
    propertyIndex: 1
  },
  {
    desc: "Tests that the strict search filter works properly for property " +
          "values",
    search: "`2%`",
    ruleCount: 2,
    propertyIndex: 0
  },
  {
    desc: "Tests that the strict search filter works properly for parsed " +
          "property names",
    search: "`color`:",
    ruleCount: 2,
    propertyIndex: 1
  },
  {
    desc: "Tests that the strict search filter works properly for parsed " +
          "property values",
    search: ":`2%`",
    ruleCount: 2,
    propertyIndex: 0
  },
  {
    desc: "Tests that the strict search filter works properly for property " +
          "line input",
    search: "`width`:`2%`",
    ruleCount: 2,
    propertyIndex: 0
  },
  {
    desc: "Tests that the search filter works properly for a parsed strict " +
          "property name and non-strict property value.",
    search: "`width`:2%",
    ruleCount: 3,
    propertyIndex: 0
  },
  {
    desc: "Tests that the search filter works properly for a parsed strict " +
          "property value and non-strict property name.",
    search: "i:`2%`",
    ruleCount: 2,
    propertyIndex: 0
  }
];

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();
  await selectNode("#testid", inspector);
  await testAddTextInFilter(inspector, view);
});

async function testAddTextInFilter(inspector, view) {
  for (const data of TEST_DATA) {
    info(data.desc);
    await setSearchFilter(view, data.search);
    await checkRules(view, data);
    await clearSearchAndCheckRules(view);
  }
}

function checkRules(view, data) {
  info("Check that the correct rules are visible");
  is(view.element.children.length, data.ruleCount,
    "Should have " + data.ruleCount + " rules.");
  is(getRuleViewRuleEditor(view, 0).rule.selectorText, "element",
    "First rule is inline element.");

  let rule = getRuleViewRuleEditor(view, 1).rule;

  is(rule.selectorText, "#testid", "Second rule is #testid.");
  ok(rule.textProps[data.propertyIndex].editor.container.classList
    .contains("ruleview-highlight"),
    "Text property is correctly highlighted.");

  if (data.ruleCount > 2) {
    rule = getRuleViewRuleEditor(view, 2).rule;
    is(rule.selectorText, ".testclass", "Third rule is .testclass.");
    ok(rule.textProps[data.propertyIndex].editor.container.classList
      .contains("ruleview-highlight"),
      "Text property is correctly highlighted.");
  }
}

async function clearSearchAndCheckRules(view) {
  const doc = view.styleDocument;
  const win = view.styleWindow;
  const searchField = view.searchField;
  const searchClearButton = view.searchClearButton;

  info("Clearing the search filter");
  EventUtils.synthesizeMouseAtCenter(searchClearButton, {}, win);
  await view.inspector.once("ruleview-filtered");

  info("Check the search filter is cleared and no rules are highlighted");
  is(view.element.children.length, 3, "Should have 3 rules.");
  ok(!searchField.value, "Search filter is cleared.");
  ok(!doc.querySelectorAll(".ruleview-highlight").length,
    "No rules are higlighted.");
}
