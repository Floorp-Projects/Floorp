/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view search filter and clear button works properly.

const TEST_URI = `
  <style type="text/css">
    #testid, h1 {
      background-color: #00F !important;
    }
    .testclass {
      width: 100%;
    }
  </style>
  <h1 id="testid" class="testclass">Styled Node</h1>
`;

const TEST_DATA = [
  {
    desc: "Tests that the search filter works properly for property names",
    search: "color"
  },
  {
    desc: "Tests that the search filter works properly for property values",
    search: "00F"
  },
  {
    desc: "Tests that the search filter works properly for property line input",
    search: "background-color:#00F"
  },
  {
    desc: "Tests that the search filter works properly for parsed property " +
          "names",
    search: "background:"
  },
  {
    desc: "Tests that the search filter works properly for parsed property " +
          "values",
    search: ":00F"
  },
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
    await checkRules(view);
    await clearSearchAndCheckRules(view);
  }
}

function checkRules(view) {
  info("Check that the correct rules are visible");
  is(view.element.children.length, 2, "Should have 2 rules.");
  is(getRuleViewRuleEditor(view, 0).rule.selectorText, "element",
    "First rule is inline element.");

  const rule = getRuleViewRuleEditor(view, 1).rule;

  is(rule.selectorText, "#testid, h1", "Second rule is #testid, h1.");
  ok(rule.textProps[0].editor.container.classList
    .contains("ruleview-highlight"),
    "background-color text property is correctly highlighted.");
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
