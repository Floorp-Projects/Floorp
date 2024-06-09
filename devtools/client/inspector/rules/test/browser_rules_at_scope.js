/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the rule-view properly handles @scope rules.

const TEST_URI = `
  <link href="${URL_ROOT_COM_SSL}doc_at_scope.css" rel="stylesheet">
  <h1>Hello @scope!</h1>
  <main>
    <style>
      @scope {
        :scope, [data-test="scoped-inline-style"] {
          border: 1px solid aqua;
        }

        div, [data-test="scoped-inline-style"] {
          background: tomato;
        }

        /* test nested @scope */
        @scope (:scope section) {
          :scope, [data-test="nested-scoped-inline-style"] {
            background: gold;
          }
        }
      }
    </style>
    <div id=a>
      inline-style scope target
      <section id="a-child">inline-style nested scope target</section>
    </div>
  </main>
  <aside>
    <div id=b>
      <span>Dough</span>
      <div id=c class="limit">
        <span>Donut hole</span>
      </div>
    </div>
  </aside>
`;

add_task(async function () {
  await pushPref("layout.css.at-scope.enabled", true);
  await addTab(
    "https://example.com/document-builder.sjs?html=" +
      encodeURIComponent(TEST_URI)
  );
  const { inspector, view } = await openRuleView();
  await assertRules("main", [
    { selector: `element`, ancestorRulesData: null },
    {
      selector: `:scope, [data-test="scoped-inline-style"]`,
      ancestorRulesData: ["@scope {"],
    },
  ]);

  await assertRules("main #a", [
    { selector: `element`, ancestorRulesData: null },
    {
      selector: `div, [data-test="scoped-inline-style"]`,
      ancestorRulesData: ["@scope {"],
    },
  ]);

  await assertRules("main #a #a-child", [
    { selector: `element`, ancestorRulesData: null },
    {
      selector: `:scope, [data-test="nested-scoped-inline-style"]`,
      ancestorRulesData: ["@scope {", "  @scope (:scope section) {"],
    },
  ]);

  await assertRules("aside #b", [
    { selector: `element`, ancestorRulesData: null },
    {
      selector: `div, [data-test="start-and-end-inherit"]`,
      ancestorRulesData: ["@scope (aside) to (.limit) {"],
    },
    {
      selector: `div, [data-test="start-and-end"]`,
      ancestorRulesData: ["@scope (aside) to (.limit) {"],
    },
    {
      selector: `div, [data-test="start-no-end"]`,
      ancestorRulesData: ["@scope (aside) {"],
    },
  ]);

  await assertRules("aside #b > span", [
    { selector: `element`, ancestorRulesData: null },
    {
      selector: `& span`,
      ancestorRulesData: [
        "@scope (aside) to (.limit) {",
        `  div, [data-test="start-and-end"] {`,
      ],
    },
    {
      selector: `div, [data-test="start-and-end-inherit"]`,
      inherited: true,
      ancestorRulesData: ["@scope (aside) to (.limit) {"],
    },
  ]);

  await assertRules("aside #c", [
    { selector: `element`, ancestorRulesData: null },
    {
      selector: `div, [data-test="start-no-end"]`,
      ancestorRulesData: ["@scope (aside) {"],
    },
    {
      selector: `div, [data-test="start-and-end-inherit"]`,
      inherited: true,
      ancestorRulesData: ["@scope (aside) to (.limit) {"],
    },
  ]);

  await assertRules("aside #c > span", [
    { selector: `element`, ancestorRulesData: null },
    {
      selector: `div, [data-test="start-and-end-inherit"]`,
      inherited: true,
      ancestorRulesData: ["@scope (aside) to (.limit) {"],
    },
  ]);

  async function assertRules(nodeSelector, expectedRules) {
    await selectNode(nodeSelector, inspector);
    const rulesInView = Array.from(
      view.element.querySelectorAll(".ruleview-rule")
    );
    is(
      rulesInView.length,
      expectedRules.length,
      `[${nodeSelector}] All expected rules are displayed`
    );

    for (let i = 0; i < expectedRules.length; i++) {
      const expectedRule = expectedRules[i];
      info(`[${nodeSelector}] Checking rule #${i}: ${expectedRule.selector}`);

      const selector = rulesInView[i].querySelector(
        ".ruleview-selectors-container"
      )?.innerText;

      is(
        selector,
        expectedRule.selector,
        `[${nodeSelector}] Expected selector for rule #${i}`
      );

      const isInherited = rulesInView[i].matches(
        ".ruleview-header-inherited + .ruleview-rule"
      );
      if (expectedRule.inherited) {
        ok(isInherited, `[${nodeSelector}] rule #${i} is inherited`);
      } else {
        ok(!isInherited, `[${nodeSelector}] rule #${i} is not inherited`);
      }

      if (expectedRule.ancestorRulesData == null) {
        is(
          getRuleViewAncestorRulesDataElementByIndex(view, i),
          null,
          `[${nodeSelector}] No ancestor rules data displayed for ${selector}`
        );
      } else {
        is(
          getRuleViewAncestorRulesDataTextByIndex(view, i),
          expectedRule.ancestorRulesData.join("\n"),
          `[${nodeSelector}] Expected ancestor rules data displayed for ${selector}`
        );
      }
    }
  }
});
