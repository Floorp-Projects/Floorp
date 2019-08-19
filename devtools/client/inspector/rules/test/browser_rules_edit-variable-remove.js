/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test removing a CSS variable.

const TEST_URI = `
  <style type='text/css'>
    div {
      color: var(--color);
      --color: lime;
    }
  </style>
  <div></div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await selectNode("div", inspector);

  info("Check the initial state of the --color variable");
  checkCSSVariableOutput(
    view,
    "div",
    "color",
    "ruleview-variable",
    "--color = lime"
  );

  info("Remove the --color variable declaration");
  const { rule } = getRuleViewRuleEditor(view, 1);
  const prop = rule.textProps[1];
  await removeProperty(view, prop);
  checkCSSVariableOutput(
    view,
    "div",
    "color",
    "ruleview-unmatched-variable",
    "--color is not set"
  );
});
