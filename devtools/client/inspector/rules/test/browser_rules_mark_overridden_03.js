/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view marks overridden rules correctly based on the
// priority for the rule

const TEST_URI = `
  <style type='text/css'>
  #testid {
    background-color: blue;
  }
  .testclass {
    background-color: green !important;
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
  ok(idProp.overridden, "Not-important rule should be overridden.");

  const classRule = getRuleViewRuleEditor(view, 2).rule;
  const classProp = classRule.textProps[0];
  ok(!classProp.overridden, "Important rule should not be overridden.");

  ok(idProp.overridden, "ID property should be overridden.");

  // FIXME: re-enable these 2 assertions when bug 1247737 is fixed.
  // let elementProp = yield addProperty(view, 0, "background-color", "purple");
  // ok(!elementProp.overridden, "New important prop should not be overriden.");
  // ok(classProp.overridden, "Class property should be overridden.");
});
