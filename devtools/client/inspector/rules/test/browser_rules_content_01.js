/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the rule-view content is correct

const TEST_URI = `
  <style type="text/css">
    @media screen and (min-width: 10px) {
      #testid {
        background-color: blue;
      }
    }
    .testclass, .unmatched {
      background-color: green;
    }
  </style>
  <div id="testid" class="testclass">Styled Node</div>
  <div id="testid2">Styled Node</div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();

  await selectNode("#testid", inspector);
  is(
    view.element.querySelectorAll("#ruleview-no-results").length,
    0,
    "After a highlight, no longer has a no-results element."
  );

  await clearCurrentNodeSelection(inspector);
  is(
    view.element.querySelectorAll("#ruleview-no-results").length,
    1,
    "After highlighting null, has a no-results element again."
  );

  await selectNode("#testid", inspector);

  let linkText = getRuleViewLinkTextByIndex(view, 1);
  is(linkText, "inline:3", "link text at index 1 has expected content.");

  const mediaText = getRuleViewAncestorRulesDataTextByIndex(view, 1);
  is(
    mediaText,
    "@media screen and (min-width: 10px)",
    "media text at index 1 has expected content"
  );

  linkText = getRuleViewLinkTextByIndex(view, 2);
  is(linkText, "inline:7", "link text at index 2 has expected content.");
  is(
    getRuleViewAncestorRulesDataElementByIndex(view, 2),
    null,
    "There is no media text element for rule at index 2"
  );

  const selector = getRuleViewRuleEditor(view, 2).selectorText;
  is(
    selector.querySelector(".ruleview-selector-matched").textContent,
    ".testclass",
    ".textclass should be matched."
  );
  is(
    selector.querySelector(".ruleview-selector-unmatched").textContent,
    ".unmatched",
    ".unmatched should not be matched."
  );
});
