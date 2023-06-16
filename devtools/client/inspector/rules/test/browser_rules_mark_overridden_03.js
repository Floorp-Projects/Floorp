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

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await selectNode("#testid", inspector);

  const idProp = getTextProperty(view, 1, { "background-color": "blue" });
  ok(idProp.overridden, "Not-important rule should be overridden.");

  const classProp = getTextProperty(view, 2, { "background-color": "green" });
  ok(!classProp.overridden, "Important rule should not be overridden.");

  ok(idProp.overridden, "ID property should be overridden.");
});
