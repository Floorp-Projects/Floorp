/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view search filter escape keypress will clear the search
// field.

const SEARCH = "00F";

const TEST_URI = `
  <style type="text/css">
    #testid {
      background-color: #00F;
    }
    .testclass {
      width: 100%;
    }
  </style>
  <div id="testid" class="testclass">Styled Node</div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await selectNode("#testid", inspector);
  await testAddTextInFilter(inspector, view);
  await testEscapeKeypress(inspector, view);
});

async function testAddTextInFilter(inspector, view) {
  await setSearchFilter(view, SEARCH);

  info("Check that the correct rules are visible");
  is(view.element.children.length, 2, "Should have 2 rules.");
  is(
    getRuleViewRuleEditor(view, 0).rule.selectorText,
    "element",
    "First rule is inline element."
  );

  const rule = getRuleViewRuleEditor(view, 1).rule;
  const prop = getTextProperty(view, 1, { "background-color": "#00F" });

  is(rule.selectorText, "#testid", "Second rule is #testid.");
  ok(
    prop.editor.container.classList.contains("ruleview-highlight"),
    "background-color text property is correctly highlighted."
  );
}

async function testEscapeKeypress(inspector, view) {
  info("Pressing the escape key on search filter");

  const doc = view.styleDocument;
  const win = view.styleWindow;
  const searchField = view.searchField;
  const onRuleViewFiltered = inspector.once("ruleview-filtered");

  searchField.focus();
  EventUtils.synthesizeKey("VK_ESCAPE", {}, win);
  await onRuleViewFiltered;

  info("Check the search filter is cleared and no rules are highlighted");
  is(view.element.children.length, 3, "Should have 3 rules.");
  ok(!searchField.value, "Search filter is cleared");
  ok(
    !doc.querySelectorAll(".ruleview-highlight").length,
    "No rules are higlighted"
  );
}
