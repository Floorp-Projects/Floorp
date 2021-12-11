/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the expanded computed list for a property remains open after
// clearing the rule view search filter.

const SEARCH = "0px";

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
  const { inspector, view } = await openRuleView();
  await selectNode("#testid", inspector);
  await testOpenExpanderAndAddTextInFilter(inspector, view);
  await testClearSearchFilter(inspector, view);
});

async function testOpenExpanderAndAddTextInFilter(inspector, view) {
  const rule = getRuleViewRuleEditor(view, 1).rule;
  const ruleEditor = getTextProperty(view, 1, { margin: "4px 0px" }).editor;
  const computed = ruleEditor.computed;

  info("Opening the computed list of margin property");
  ruleEditor.expander.click();

  await setSearchFilter(view, SEARCH);

  info("Check that the correct rules are visible");
  is(view.element.children.length, 2, "Should have 2 rules.");
  is(
    getRuleViewRuleEditor(view, 0).rule.selectorText,
    "element",
    "First rule is inline element."
  );

  is(rule.selectorText, "#testid", "Second rule is #testid.");
  ok(ruleEditor.expander.getAttribute("open"), "Expander is open.");
  ok(
    ruleEditor.container.classList.contains("ruleview-highlight"),
    "margin text property is correctly highlighted."
  );
  ok(
    !computed.hasAttribute("filter-open"),
    "margin computed list does not contain filter-open class."
  );
  ok(
    computed.hasAttribute("user-open"),
    "margin computed list contains user-open attribute."
  );

  ok(
    !computed.children[0].classList.contains("ruleview-highlight"),
    "margin-top computed property is not highlighted."
  );
  ok(
    computed.children[1].classList.contains("ruleview-highlight"),
    "margin-right computed property is correctly highlighted."
  );
  ok(
    !computed.children[2].classList.contains("ruleview-highlight"),
    "margin-bottom computed property is not highlighted."
  );
  ok(
    computed.children[3].classList.contains("ruleview-highlight"),
    "margin-left computed property is correctly highlighted."
  );
}

async function testClearSearchFilter(inspector, view) {
  info("Clearing the search filter");

  const searchField = view.searchField;
  const searchClearButton = view.searchClearButton;
  const onRuleViewFiltered = inspector.once("ruleview-filtered");

  EventUtils.synthesizeMouseAtCenter(searchClearButton, {}, view.styleWindow);

  await onRuleViewFiltered;

  info("Check the search filter is cleared and no rules are highlighted");
  is(view.element.children.length, 3, "Should have 3 rules.");
  ok(!searchField.value, "Search filter is cleared");
  ok(
    !view.styleDocument.querySelectorAll(".ruleview-highlight").length,
    "No rules are higlighted"
  );

  const ruleEditor = getRuleViewRuleEditor(view, 1).rule.textProps[0].editor;
  const computed = ruleEditor.computed;

  ok(ruleEditor.expander.getAttribute("open"), "Expander is open.");
  ok(
    !computed.hasAttribute("filter-open"),
    "margin computed list does not contain filter-open class."
  );
  ok(
    computed.hasAttribute("user-open"),
    "margin computed list contains user-open attribute."
  );
}
