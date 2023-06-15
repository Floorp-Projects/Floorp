/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the rule-view content is correct when the page uses nested CSS rules
const TEST_URI = `
  <style type="text/css">
    body {
      background: tomato;
      container-type: inline-size;

      @media screen {
        container-name: main;

        & h1 {
          border-color: gold;

          .foo {
            color: white;
          }

          #bar {
            text-decoration: underline;
          }

          @container main (width > 10px) {
            & + nav {
              border: 1px solid;

              [href] {
                background-color: lightgreen;
              }
            }
          }
        }
      }
    }
  </style>
  <h1>Hello <i class="foo">nested</i> <em id="bar">rules</em>!</h1>
  <nav>
    <ul>
      <li><a href="#">Leaf</a></li>
      <li><a>Nowhere</a></li>
    </ul>
  </nav>
`;

add_task(async function () {
  await pushPref("layout.css.nesting.enabled", true);

  await addTab(
    "https://example.com/document-builder.sjs?html=" +
      encodeURIComponent(TEST_URI)
  );
  const { inspector, view } = await openRuleView();

  await selectNode("body", inspector);
  checkRuleSelectorAndAncestorData(view, [
    { selector: "element", ancestorRulesData: null },
    {
      selector: `&`,
      ancestorRulesData: [`body`, `@media screen`],
    },
    {
      selector: `body`,
      ancestorRulesData: null,
    },
  ]);

  await selectNode("h1", inspector);
  checkRuleSelectorAndAncestorData(view, [
    { selector: "element", ancestorRulesData: null },
    {
      selector: `& h1`,
      ancestorRulesData: [`body`, `@media screen`],
    },
  ]);

  await selectNode("h1 > .foo", inspector);
  checkRuleSelectorAndAncestorData(view, [
    { selector: "element", ancestorRulesData: null },
    {
      selector: `.foo`,
      ancestorRulesData: [`body`, `@media screen`, `& h1`],
    },
  ]);

  await selectNode("h1 > #bar", inspector);
  checkRuleSelectorAndAncestorData(view, [
    { selector: "element", ancestorRulesData: null },
    {
      selector: `#bar`,
      ancestorRulesData: [`body`, `@media screen`, `& h1`],
    },
  ]);

  await selectNode("nav", inspector);
  checkRuleSelectorAndAncestorData(view, [
    { selector: "element", ancestorRulesData: null },
    {
      selector: `& + nav`,
      ancestorRulesData: [
        `body`,
        `@media screen`,
        `& h1`,
        `@container main (width > 10px)`,
      ],
    },
  ]);

  await selectNode("nav a", inspector);
  checkRuleSelectorAndAncestorData(view, [
    { selector: "element", ancestorRulesData: null },
    {
      selector: `[href]`,
      ancestorRulesData: [
        `body`,
        `@media screen`,
        `& h1`,
        `@container main (width > 10px)`,
        `& + nav`,
      ],
    },
  ]);
});

function checkRuleSelectorAndAncestorData(view, expectedRules) {
  const rulesInView = Array.from(view.element.children);
  is(
    rulesInView.length,
    expectedRules.length,
    "All expected rules are displayed"
  );

  for (let i = 0; i < expectedRules.length; i++) {
    const expectedRule = expectedRules[i];
    info(`Checking rule #${i}: ${expectedRule.selector}`);

    const selector = rulesInView[i].querySelector(
      ".ruleview-selectorcontainer"
    ).innerText;
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
}
