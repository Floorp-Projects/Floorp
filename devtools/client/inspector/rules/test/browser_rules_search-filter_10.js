/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view search filter works properly for rule selectors.

const TEST_URI = `
  <style type="text/css">
    html, body, div {
      background-color: #00F;
    }
    #testid {
      width: 100%;
    }
  </style>
  <div id="testid" class="testclass">Styled Node</div>
`;

const TEST_DATA = [
  {
    desc: "Tests that the search filter works properly for a single rule " +
          "selector",
    search: "#test",
    selectorText: "#testid",
    index: 0
  },
  {
    desc: "Tests that the search filter works properly for multiple rule " +
          "selectors",
    search: "body",
    selectorText: "html, body, div",
    index: 2
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
  is(view.element.children.length, 2, "Should have 2 rules.");
  is(getRuleViewRuleEditor(view, 0).rule.selectorText, "element",
    "First rule is inline element.");

  const ruleEditor = getRuleViewRuleEditor(view, 1);

  is(ruleEditor.rule.selectorText, data.selectorText,
    "Second rule is " + data.selectorText + ".");
  ok(ruleEditor.selectorText.children[data.index].classList
    .contains("ruleview-highlight"),
    data.selectorText + " selector is highlighted.");
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
