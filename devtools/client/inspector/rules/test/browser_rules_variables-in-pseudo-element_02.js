/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for pseudo element which inherits CSS variable.

const TEST_URI = `
  <style type='text/css'>
    div::before {
      color: var(--color);
    }

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

  info("Test the CSS variable which normal element is referring to");
  checkCSSVariableOutput(
    view,
    "div",
    "color",
    "ruleview-variable",
    "--color = lime"
  );

  info("Test the CSS variable which pseudo element is referring to");
  checkCSSVariableOutput(
    view,
    "div::before",
    "color",
    "ruleview-variable",
    "--color = lime"
  );
});
