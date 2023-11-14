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
  await addTab(
    "https://example.com/document-builder.sjs?html=" +
      encodeURIComponent(TEST_URI)
  );
  const { inspector, view } = await openRuleView();

  await selectNode("body", inspector);
  checkRuleViewContent(view, [
    { selector: "element", ancestorRulesData: null, declarations: [] },
    {
      selector: `&`,
      // prettier-ignore
      ancestorRulesData: [
        `body {`,
        `  @media screen {`
      ],
      declarations: [{ name: "container-name", value: "main" }],
    },
    {
      selector: `body`,
      ancestorRulesData: null,
      declarations: [
        { name: "background", value: "tomato" },
        { name: "container-type", value: "inline-size" },
      ],
    },
  ]);

  await selectNode("h1", inspector);
  checkRuleViewContent(view, [
    { selector: "element", ancestorRulesData: null, declarations: [] },
    {
      selector: `& h1`,
      // prettier-ignore
      ancestorRulesData: [
        `body {`,
        `  @media screen {`
      ],
      declarations: [{ name: "border-color", value: "gold" }],
    },
  ]);

  await selectNode("h1 > .foo", inspector);
  checkRuleViewContent(view, [
    { selector: "element", ancestorRulesData: null, declarations: [] },
    {
      selector: `.foo`,
      // prettier-ignore
      ancestorRulesData: [
        `body {`,
        `  @media screen {`,
        `    & h1 {`
      ],
      declarations: [{ name: "color", value: "white" }],
    },
  ]);

  await selectNode("h1 > #bar", inspector);
  checkRuleViewContent(view, [
    { selector: "element", ancestorRulesData: null, declarations: [] },
    {
      selector: `#bar`,
      // prettier-ignore
      ancestorRulesData: [
        `body {`,
        `  @media screen {`,
        `    & h1 {`
      ],
      declarations: [{ name: "text-decoration", value: "underline" }],
    },
  ]);

  await selectNode("nav", inspector);
  checkRuleViewContent(view, [
    { selector: "element", ancestorRulesData: null, declarations: [] },
    {
      selector: `& + nav`,
      ancestorRulesData: [
        `body {`,
        `  @media screen {`,
        `    & h1 {`,
        `      @container main (width > 10px) {`,
      ],
      declarations: [{ name: "border", value: "1px solid" }],
    },
  ]);

  await selectNode("nav a", inspector);
  checkRuleViewContent(view, [
    { selector: "element", ancestorRulesData: null, declarations: [] },
    {
      selector: `[href]`,
      ancestorRulesData: [
        `body {`,
        `  @media screen {`,
        `    & h1 {`,
        `      @container main (width > 10px) {`,
        `        & + nav {`,
      ],
      declarations: [{ name: "background-color", value: "lightgreen" }],
    },
  ]);
});

function checkRuleViewContent(view, expectedRules) {
  const rulesInView = Array.from(view.element.children);
  is(
    rulesInView.length,
    expectedRules.length,
    "All expected rules are displayed"
  );

  for (let i = 0; i < expectedRules.length; i++) {
    const expectedRule = expectedRules[i];
    info(`Checking rule #${i}: ${expectedRule.selector}`);

    const ruleInView = rulesInView[i];
    const selector = ruleInView.querySelector(
      ".ruleview-selectors-container"
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

    const declarations = ruleInView.querySelectorAll(".ruleview-property");
    is(
      declarations.length,
      expectedRule.declarations.length,
      "Got the expected number of declarations"
    );
    for (let j = 0; j < declarations.length; j++) {
      const expectedDeclaration = expectedRule.declarations[j];
      const [propName, propValue] = Array.from(
        declarations[j].querySelectorAll(
          ".ruleview-propertyname, .ruleview-propertyvalue"
        )
      );
      is(
        propName.innerText,
        expectedDeclaration?.name,
        "Got expected property name"
      );
      is(
        propValue.innerText,
        expectedDeclaration?.value,
        "Got expected property value"
      );
    }
  }
}
