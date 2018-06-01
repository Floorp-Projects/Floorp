/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view marks overridden rules correctly for short hand
// properties and the computed list properties

const TEST_URI = `
  <style type='text/css'>
  #testid {
    margin-left: 1px;
  }
  .testclass {
    margin: 2px;
  }
  </style>
  <div id='testid' class='testclass'>Styled Node</div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();
  await selectNode("#testid", inspector);
  await testMarkOverridden(inspector, view);
});

function testMarkOverridden(inspector, view) {
  const elementStyle = view._elementStyle;

  const classRule = elementStyle.rules[2];
  const classProp = classRule.textProps[0];
  ok(!classProp.overridden,
    "Class prop shouldn't be overridden, some props are still being used.");

  for (const computed of classProp.computed) {
    if (computed.name.indexOf("margin-left") == 0) {
      ok(computed.overridden, "margin-left props should be overridden.");
    } else {
      ok(!computed.overridden,
        "Non-margin-left props should not be overridden.");
    }
  }
}
