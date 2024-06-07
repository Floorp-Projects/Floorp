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

    main {
      container-type: inline-size;

      & > .foo, .unmatched {
        color: tomato;

        @container (0px < width) {
          background: gold;
        }
      }
    }

    #specific,
    #specific.test,
    aside#specific.test,
    body#bdy aside#specific.test,
    aside#specific.test:is(.this,.that) {}
  </style>
  <body id="bdy">
    <div id="testid" class="testclass">Styled Node</div>
    <main>
      <div class="foo">Styled Node in Nested rule</div>
    </main>
    <aside id=specific class="test">Test with multiple selectors</div>
  </body>
`;

add_task(async function () {
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
    "@media screen and (min-width: 10px) {",
    "media text at index 1 has expected content"
  );

  linkText = getRuleViewLinkTextByIndex(view, 2);
  is(linkText, "inline:7", "link text at index 2 has expected content.");
  is(
    getRuleViewAncestorRulesDataElementByIndex(view, 2),
    null,
    "There is no media text element for rule at index 2"
  );

  assertSelectors(view, 2, [
    {
      selector: ".testclass",
      matches: true,
      specificity: "(0,1,0)",
    },
    {
      selector: ".unmatched",
      matches: false,
      specificity: "(0,1,0)",
    },
  ]);

  info("Check nested rules");
  await selectNode(".foo", inspector);

  assertSelectors(view, 1, [
    // That's the rule that was created as a result of a
    // nested container rule (`@container (0px < width) { background: gold}`)
    // In such case, the rule's selector is only `&`, and it should be displayed as
    // matching the selected node (`<div class="foo">`).
    {
      selector: "&",
      matches: true,
      specificity: "(0,1,1)",
    },
  ]);

  assertSelectors(view, 2, [
    {
      selector: "& > .foo",
      matches: true,
      specificity: "(0,1,1)",
    },
    {
      selector: "& .unmatched",
      matches: false,
      specificity: "(0,1,1)",
    },
  ]);

  info("Check rule with multiple selectors and different specificites");
  await selectNode("#specific", inspector);
  assertSelectors(view, 1, [
    { selector: "#specific", matches: true, specificity: "(1,0,0)" },
    { selector: "#specific.test", matches: true, specificity: "(1,1,0)" },
    {
      selector: "aside#specific.test",
      matches: true,
      specificity: "(1,1,1)",
    },
    {
      selector: "body#bdy aside#specific.test",
      matches: true,
      specificity: "(2,1,2)",
    },
    {
      selector: "aside#specific.test:is(.this, .that)",
      matches: false,
      specificity: "(1,2,1)",
    },
  ]);
});

/**
 * Returns the selector elements for a given rule index
 *
 * @param {Inspector} view
 * @param {Integer} ruleIndex
 * @param {Array<Object>} expectedSelectors:
 *        An array of objects representing each selector. Objects have the following shape:
 *        - selector: The expected selector text
 *        - matches: True if the selector should have the "matching" class
 */
function assertSelectors(view, ruleIndex, expectedSelectors) {
  const ruleSelectors = getRuleViewRuleEditor(
    view,
    ruleIndex
  ).selectorText.querySelectorAll(".ruleview-selector");

  is(
    ruleSelectors.length,
    expectedSelectors.length,
    `There are the expected number of selectors on rule #${ruleIndex}`
  );

  for (let i = 0; i < expectedSelectors.length; i++) {
    const ruleSelector = ruleSelectors[i];
    const expectedSelector = expectedSelectors[i];
    const selectorText = ruleSelector.textContent;
    is(
      selectorText,
      expectedSelector.selector,
      `Got expected text for the selector element #${i} on rule #${ruleIndex}`
    );
    is(
      [...ruleSelector.classList].join(","),
      "ruleview-selector," +
        (expectedSelector.matches ? "matched" : "unmatched"),
      `Got expected css class on the selector element #${i} ("${selectorText}") on rule #${ruleIndex}`
    );
    is(
      ruleSelector.title,
      `Specificity: ${expectedSelector.specificity}`,
      `Got expected title with specificity on the selector element #${i} ("${selectorText}") on rule #${ruleIndex}`
    );
  }
}
