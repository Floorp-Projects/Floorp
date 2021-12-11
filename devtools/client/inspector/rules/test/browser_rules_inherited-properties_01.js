/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that inherited properties appear for a nested element in the
// rule view.

const TEST_URI = `
  <style type="text/css">
    #test2 {
      background-color: green;
      color: purple;
    }
  </style>
  <div id="test2"><div id="test1">Styled Node</div></div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await selectNode("#test1", inspector);
  await simpleInherit(inspector, view);
});

function simpleInherit(inspector, view) {
  const elementStyle = view._elementStyle;
  is(elementStyle.rules.length, 2, "Should have 2 rules.");

  const elementRule = elementStyle.rules[0];
  ok(
    !elementRule.inherited,
    "Element style attribute should not consider itself inherited."
  );

  const inheritRule = elementStyle.rules[1];
  is(
    inheritRule.selectorText,
    "#test2",
    "Inherited rule should be the one that includes inheritable properties."
  );
  ok(!!inheritRule.inherited, "Rule should consider itself inherited.");
  is(inheritRule.textProps.length, 2, "Rule should have two styles");
  const bgcProp = inheritRule.textProps[0];
  is(
    bgcProp.name,
    "background-color",
    "background-color property should exist"
  );
  ok(bgcProp.invisible, "background-color property should be invisible");
  const inheritProp = inheritRule.textProps[1];
  is(inheritProp.name, "color", "color should have been inherited.");
}
