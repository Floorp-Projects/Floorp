/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view shorthand overridden list works correctly,
// can be shown and hidden correctly, and contain the right subproperties.

var TEST_URI = `
  <style type="text/css">
    div {
      margin: 0px 1px 2px 3px;
      top: 0px;
    }
    #testid {
        margin-left: 10px;
        margin-right: 10px;
    }
  </style>
  <div id="testid">Styled Node</div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();
  await selectNode("#testid", inspector);
  await testComputedList(inspector, view);
});

function testComputedList(inspector, view) {
  const rule = getRuleViewRuleEditor(view, 2).rule;
  const propEditor = rule.textProps[0].editor;
  const expander = propEditor.expander;
  const overriddenItems = propEditor.shorthandOverridden.children;
  const propNames = [
    "margin-right",
    "margin-left"
  ];

  ok(!expander.hasAttribute("open"), "margin computed list is closed.");
  ok(!propEditor.shorthandOverridden.hasAttribute("hidden"),
      "The shorthandOverridden list should be open.");

  is(overriddenItems.length, propNames.length,
      "There should be 2 overridden shorthand value.");
  for (let i = 0; i < propNames.length; i++) {
    const overriddenItem = overriddenItems[i].querySelector(".ruleview-propertyname");
    is(overriddenItem.textContent, propNames[i],
        "The overridden item #" + i + " should be " + propNames[i]);
  }

  info("Opening the computed list of margin property.");
  expander.click();
  ok(expander.hasAttribute("open"), "margin computed list is open.");
  ok(propEditor.shorthandOverridden.hasAttribute("hidden"),
      "The shorthandOverridden list should be hidden.");

  info("Closing the computed list of margin property.");
  expander.click();
  ok(!expander.hasAttribute("open"), "margin computed list is closed.");
  ok(!propEditor.shorthandOverridden.hasAttribute("hidden"),
      "The shorthandOverridden list should be open.");

  for (let i = 0; i < propNames.length; i++) {
    const overriddenItem = overriddenItems[i].querySelector(".ruleview-propertyname");
    is(overriddenItem.textContent, propNames[i],
        "The overridden item #" + i + " should still be " + propNames[i]);
  }
}
