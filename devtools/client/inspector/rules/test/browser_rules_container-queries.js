/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the rule-view content is correct when the page defines container queries.
const TEST_URI = `
  <style type="text/css">
    body {
      container: mycontainer / inline-size;
    }

    @container (width > 0px) {
      h1, [test-hint="nocontainername"]{
        outline-color: chartreuse;
      }
    }

    @container unknowncontainer (min-width: 2vw) {
      h1, [test-hint="unknowncontainer"] {
        border-color: salmon;
      }
    }

    @container mycontainer (1px < width < 10000px) {
      h1, [test-hint="container"] {
        color: tomato;
      }
    }

    @layer mylayer {
      @container mycontainer (min-width: 3em) {
        @media screen {
          @container mycontainer (min-width: 4rem) {
            h1, [test-hint="nested"] {
              background: gold;
            }
          }
        }
      }
    }
  </style>
  <h1>Hello @container!</h1>
`;

add_task(async function() {
  await pushPref("layout.css.container-queries.enabled", true);

  await addTab(
    "https://example.com/document-builder.sjs?html=" +
      encodeURIComponent(TEST_URI)
  );
  const { inspector, view } = await openRuleView();

  await selectNode("h1", inspector);

  const expectedRules = [
    { selector: "element", ancestorRulesData: null },
    {
      selector: `h1, [test-hint="container"]`,
      ancestorRulesData: ["@container mycontainer (1px < width < 10000px)"],
    },
    {
      selector: `h1, [test-hint="unknowncontainer"]`,
      ancestorRulesData: ["@container unknowncontainer (min-width: 2vw)"],
    },
    {
      selector: `h1, [test-hint="nocontainername"]`,
      ancestorRulesData: ["@container (width > 0px)"],
    },
    {
      selector: `h1, [test-hint="nested"]`,
      ancestorRulesData: [
        `@layer mylayer`,
        `@container mycontainer (min-width: 3em)`,
        `@media screen`,
        `@container mycontainer (min-width: 4rem)`,
      ],
    },
  ];

  const rulesInView = Array.from(view.element.children);
  is(
    rulesInView.length,
    expectedRules.length,
    "All expected rules are displayed"
  );

  for (let i = 0; i < expectedRules.length; i++) {
    const expectedRule = expectedRules[i];
    info(`Checking rule #${i}: ${expectedRule.selector}`);

    const selector = rulesInView[i].querySelector(".ruleview-selectorcontainer")
      .innerText;
    is(selector, expectedRule.selector, `Expected selector for ${selector}`);

    if (expectedRule.ancestorRulesData == null) {
      is(
        getRuleViewAncestorRulesDataElementByIndex(view, i),
        null,
        `No ancestor rules data displayed for ${selector}`
      );
    } else {
      is(
        getRuleViewAncestorRulesDataTextByIndex(view, i),
        expectedRule.ancestorRulesData.join("\n"),
        `Expected ancestor rules data displayed for ${selector}`
      );
    }
  }
});
