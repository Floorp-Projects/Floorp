/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view marks overridden rules correctly based on the
// specificity of the rule.

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

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await selectNode("#testid", inspector);

  const idProp = getTextProperty(view, 1, { "background-color": "blue" });
  ok(!idProp.overridden, "ID prop should not be overridden.");
  ok(
    !idProp.editor.element.classList.contains("ruleview-overridden"),
    "ID property editor should not have ruleview-overridden class"
  );

  const classProp = getTextProperty(view, 2, { "background-color": "green" });
  ok(classProp.overridden, "Class property should be overridden.");
  ok(
    classProp.editor.element.classList.contains("ruleview-overridden"),
    "Class property editor should have ruleview-overridden class"
  );

  // Override background-color by changing the element style.
  const elementProp = await addProperty(view, 0, "background-color", "purple");

  ok(
    !elementProp.overridden,
    "Element style property should not be overridden"
  );
  ok(idProp.overridden, "ID property should be overridden");
  ok(
    idProp.editor.element.classList.contains("ruleview-overridden"),
    "ID property editor should have ruleview-overridden class"
  );
  ok(classProp.overridden, "Class property should be overridden");
  ok(
    classProp.editor.element.classList.contains("ruleview-overridden"),
    "Class property editor should have ruleview-overridden class"
  );
});
