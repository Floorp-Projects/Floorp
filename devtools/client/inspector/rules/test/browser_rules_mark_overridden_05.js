/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view marks overridden rules correctly based on the
// order of the property.

const TEST_URI = `
  <style type='text/css'>
  #testid {
    background-color: green;
  }
  </style>
  <div id='testid' class='testclass'>Styled Node</div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await selectNode("#testid", inspector);

  await addProperty(view, 1, "background-color", "red");

  const firstProp = getTextProperty(view, 1, { "background-color": "green" });
  const secondProp = getTextProperty(view, 1, { "background-color": "red" });

  ok(firstProp.overridden, "First property should be overridden.");
  ok(!secondProp.overridden, "Second property should not be overridden.");
});
