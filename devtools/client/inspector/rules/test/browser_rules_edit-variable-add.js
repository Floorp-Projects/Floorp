/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test adding a CSS variable.

const TEST_URI = `
  <style type='text/css'>
    div {
      color: var(--color);
    }
  </style>
  <div></div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await selectNode("div", inspector);

  info("Check the initial state of --color which refers to an unset variable");
  checkCSSVariableOutput(
    view,
    "div",
    "color",
    "ruleview-unmatched-variable",
    "--color is not set"
  );

  info("Add the --color CSS variable");
  await addProperty(view, 0, "--color", "lime");
  checkCSSVariableOutput(
    view,
    "div",
    "color",
    "ruleview-variable",
    "--color = lime"
  );
});
