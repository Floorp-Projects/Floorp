/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view strict search filter and clear button works properly
// in the computed list

const TEST_URI = `
  <style type="text/css">
    #testid {
      margin: 4px 0px 10px 44px;
    }
    .testclass {
      background-color: red;
    }
  </style>
  <h1 id="testid" class="testclass">Styled Node</h1>
`;

const TEST_DATA = [
  {
    desc:
      "Tests that the strict search filter works properly in the " +
      "computed list for property names",
    search: "`margin-left`",
    isExpanderOpen: true,
    isFilterOpen: true,
    isMarginHighlighted: false,
    isMarginTopHighlighted: false,
    isMarginRightHighlighted: false,
    isMarginBottomHighlighted: false,
    isMarginLeftHighlighted: true,
  },
  {
    desc:
      "Tests that the strict search filter works properly in the " +
      "computed list for property values",
    search: "`0px`",
    isExpanderOpen: true,
    isFilterOpen: true,
    isMarginHighlighted: false,
    isMarginTopHighlighted: false,
    isMarginRightHighlighted: true,
    isMarginBottomHighlighted: false,
    isMarginLeftHighlighted: false,
  },
  {
    desc:
      "Tests that the strict search filter works properly in the " +
      "computed list for parsed property names",
    search: "`margin-left`:",
    isExpanderOpen: true,
    isFilterOpen: true,
    isMarginHighlighted: false,
    isMarginTopHighlighted: false,
    isMarginRightHighlighted: false,
    isMarginBottomHighlighted: false,
    isMarginLeftHighlighted: true,
  },
  {
    desc:
      "Tests that the strict search filter works properly in the " +
      "computed list for parsed property values",
    search: ":`4px`",
    isExpanderOpen: true,
    isFilterOpen: true,
    isMarginHighlighted: false,
    isMarginTopHighlighted: true,
    isMarginRightHighlighted: false,
    isMarginBottomHighlighted: false,
    isMarginLeftHighlighted: false,
  },
  {
    desc:
      "Tests that the strict search filter works properly in the " +
      "computed list for property line input",
    search: "`margin-top`:`4px`",
    isExpanderOpen: true,
    isFilterOpen: true,
    isMarginHighlighted: false,
    isMarginTopHighlighted: true,
    isMarginRightHighlighted: false,
    isMarginBottomHighlighted: false,
    isMarginLeftHighlighted: false,
  },
  {
    desc:
      "Tests that the strict search filter works properly in the " +
      "computed list for a parsed strict property name and non-strict " +
      "property value",
    search: "`margin-top`:4px",
    isExpanderOpen: true,
    isFilterOpen: true,
    isMarginHighlighted: false,
    isMarginTopHighlighted: true,
    isMarginRightHighlighted: false,
    isMarginBottomHighlighted: false,
    isMarginLeftHighlighted: false,
  },
  {
    desc:
      "Tests that the strict search filter works properly in the " +
      "computed list for a parsed strict property value and non-strict " +
      "property name",
    search: "i:`4px`",
    isExpanderOpen: true,
    isFilterOpen: true,
    isMarginHighlighted: false,
    isMarginTopHighlighted: true,
    isMarginRightHighlighted: false,
    isMarginBottomHighlighted: false,
    isMarginLeftHighlighted: false,
  },
];

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
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
  is(
    getRuleViewRuleEditor(view, 0).rule.selectorText,
    "element",
    "First rule is inline element."
  );

  const rule = getRuleViewRuleEditor(view, 1).rule;
  const textPropEditor = rule.textProps[0].editor;
  const computed = textPropEditor.computed;

  is(rule.selectorText, "#testid", "Second rule is #testid.");
  is(
    !!textPropEditor.expander.getAttribute("open"),
    data.isExpanderOpen,
    "Got correct expander state."
  );
  is(
    computed.hasAttribute("filter-open"),
    data.isFilterOpen,
    "Got correct expanded state for margin computed list."
  );
  is(
    textPropEditor.container.classList.contains("ruleview-highlight"),
    data.isMarginHighlighted,
    "Got correct highlight for margin text property."
  );

  is(
    computed.children[0].classList.contains("ruleview-highlight"),
    data.isMarginTopHighlighted,
    "Got correct highlight for margin-top computed property."
  );
  is(
    computed.children[1].classList.contains("ruleview-highlight"),
    data.isMarginRightHighlighted,
    "Got correct highlight for margin-right computed property."
  );
  is(
    computed.children[2].classList.contains("ruleview-highlight"),
    data.isMarginBottomHighlighted,
    "Got correct highlight for margin-bottom computed property."
  );
  is(
    computed.children[3].classList.contains("ruleview-highlight"),
    data.isMarginLeftHighlighted,
    "Got correct highlight for margin-left computed property."
  );
}

async function clearSearchAndCheckRules(view) {
  const win = view.styleWindow;
  const searchField = view.searchField;
  const searchClearButton = view.searchClearButton;

  const rule = getRuleViewRuleEditor(view, 1).rule;
  const textPropEditor = rule.textProps[0].editor;
  const computed = textPropEditor.computed;

  info("Clearing the search filter");
  EventUtils.synthesizeMouseAtCenter(searchClearButton, {}, win);
  await view.inspector.once("ruleview-filtered");

  info("Check the search filter is cleared and no rules are highlighted");
  is(view.element.children.length, 3, "Should have 3 rules.");
  ok(!searchField.value, "Search filter is cleared");
  ok(
    !view.styleDocument.querySelectorAll(".ruleview-highlight").length,
    "No rules are higlighted"
  );

  ok(!textPropEditor.expander.getAttribute("open"), "Expander is closed.");
  ok(!computed.hasAttribute("filter-open"), "margin computed list is closed.");
}
