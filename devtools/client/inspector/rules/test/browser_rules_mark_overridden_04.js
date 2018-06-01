/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view marks overridden rules correctly if a property gets
// disabled

const TEST_URI = `
  <style type='text/css'>
  #testid {
    background-color: blue;
  }
  .testclass {
    background-color: green;
  }
  </style>
  <div id='testid' class='testclass'>Styled Node</div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();
  await selectNode("#testid", inspector);

  const idRule = getRuleViewRuleEditor(view, 1).rule;
  const idProp = idRule.textProps[0];

  await togglePropStatus(view, idProp);

  const classRule = getRuleViewRuleEditor(view, 2).rule;
  const classProp = classRule.textProps[0];
  ok(!classProp.overridden,
    "Class prop should not be overridden after id prop was disabled.");
});
