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
    desc: "Tests that the search filter works properly for a single rule selector",
    search: "#test",
    selectorText: "#testid",
    index: 0
  },
  {
    desc: "Tests that the search filter works properly for multiple rule selectors",
    search: "body",
    selectorText: "html, body, div",
    index: 2
  }
];

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);
  yield testAddTextInFilter(inspector, view);
});

function* testAddTextInFilter(inspector, view) {
  for (let data of TEST_DATA) {
    info(data.desc);
    yield setSearchFilter(view, data.search);
    yield checkRules(view, data);
    yield clearSearchAndCheckRules(view);
  }
}

function* checkRules(view, data) {
  info("Check that the correct rules are visible");
  is(view.element.children.length, 2, "Should have 2 rules.");
  is(getRuleViewRuleEditor(view, 0).rule.selectorText, "element",
    "First rule is inline element.");

  let ruleEditor = getRuleViewRuleEditor(view, 1);

  is(ruleEditor.rule.selectorText, data.selectorText,
    "Second rule is " + data.selectorText + ".");
  ok(ruleEditor.selectorText.children[data.index].classList
    .contains("ruleview-highlight"),
    data.selectorText + " selector is highlighted.");
}

function* clearSearchAndCheckRules(view) {
  let doc = view.styleDocument;
  let win = view.styleWindow;
  let searchField = view.searchField;
  let searchClearButton = view.searchClearButton;

  info("Clearing the search filter");
  EventUtils.synthesizeMouseAtCenter(searchClearButton, {}, win);
  yield view.inspector.once("ruleview-filtered");

  info("Check the search filter is cleared and no rules are highlighted");
  is(view.element.children.length, 3, "Should have 3 rules.");
  ok(!searchField.value, "Search filter is cleared.");
  ok(!doc.querySelectorAll(".ruleview-highlight").length,
    "No rules are higlighted.");
}
